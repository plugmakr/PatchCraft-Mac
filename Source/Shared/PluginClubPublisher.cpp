#include "PluginClubPublisher.h"

#include "LicenseValidator.h"
#include "PatchCraftPackWriter.h"
#include "PluginClubApi.h"

#include <cstdlib>
#include <initializer_list>

namespace patchcraft
{
    namespace
    {
        // Canonical publish endpoint (live): POST https://plugin.club/functions/v1/sellerImport
        // Auth: Bearer access_token from deviceAuthStart/Poll (docs). Optional PLUGINCLUB_API_KEY fallback.
        static constexpr auto kPluginClubDefaultFunctionsEndpoint = PluginClubApi::kBaseUrl;
        static constexpr auto kPluginClubSellerImportPath = PluginClubApi::kSellerImportPath;
        static constexpr auto kLegacyRomplurSellerImportPath = "romplurSellerImport";

        static juce::String safeSlug (juce::String text)
        {
            text = text.trim();
            if (text.isEmpty())
                text = "patchcraft-pack";
            return text.toLowerCase().replaceCharacters (" \\/:*?\"<>|", "__________");
        }

        static juce::String ensureTrailingSlashRemoved (juce::String text)
        {
            return text.trim().trimCharactersAtEnd ("/");
        }

        static bool isPluginClubUrl (const juce::String& text)
        {
            const auto lower = text.toLowerCase();
            return lower.startsWith ("https://plugin.club")
                || lower.startsWith ("https://www.plugin.club");
        }

        static juce::String ensureHttpsScheme (juce::String value)
        {
            value = value.trim();
            if (value.startsWithIgnoreCase ("http://"))
                value = "https://" + value.substring (7);
            else if (! value.startsWithIgnoreCase ("https://") && value.containsChar ('.'))
                value = "https://" + value;
            return value;
        }

        static juce::String ensurePluginClubFunctionsPrefix (juce::String endpoint)
        {
            endpoint = ensureTrailingSlashRemoved (ensureHttpsScheme (endpoint));
            if (! isPluginClubUrl (endpoint))
                return endpoint;

            if (endpoint.containsIgnoreCase ("/functions/v1/")
                || endpoint.endsWithIgnoreCase ("/functions/v1"))
                return endpoint;

            if (endpoint.containsIgnoreCase ("/functions/") && ! endpoint.containsIgnoreCase ("/functions/v1/"))
            {
                return endpoint.replace ("https://www.plugin.club/functions/", "https://plugin.club/functions/v1/")
                               .replace ("https://plugin.club/functions/", "https://plugin.club/functions/v1/");
            }

            if (endpoint.endsWithIgnoreCase ("/functions"))
                return ensureTrailingSlashRemoved (endpoint) + "/v1";

            if (endpoint.startsWithIgnoreCase ("https://www.plugin.club/"))
                return endpoint.replace ("https://www.plugin.club/", "https://plugin.club/functions/v1/");

            if (endpoint.startsWithIgnoreCase ("https://plugin.club/"))
                return endpoint.replace ("https://plugin.club/", "https://plugin.club/functions/v1/");

            return endpoint;
        }

        static juce::String stripKnownSellerSuffix (juce::String value)
        {
            value = ensureTrailingSlashRemoved (value);
            if (value.endsWithIgnoreCase ("/" + juce::String (kPluginClubSellerImportPath)))
                return ensureTrailingSlashRemoved (value.dropLastCharacters ((int) std::strlen (kPluginClubSellerImportPath) + 1));
            if (value.endsWithIgnoreCase ("/" + juce::String (kLegacyRomplurSellerImportPath)))
                return ensureTrailingSlashRemoved (value.dropLastCharacters ((int) std::strlen (kLegacyRomplurSellerImportPath) + 1));
            return value;
        }

        static bool looksLikeHtmlResponse (const juce::String& text)
        {
            const auto trimmed = text.trimStart();
            return trimmed.startsWithIgnoreCase ("<!doctype html")
                || trimmed.startsWithIgnoreCase ("<html")
                || trimmed.containsIgnoreCase ("<script")
                || trimmed.containsIgnoreCase ("text/html");
        }

        static juce::StringArray candidateEndpointsFor (const juce::String& configuredEndpoint)
        {
            juce::StringArray endpoints;
            auto add = [&endpoints] (juce::String endpoint)
            {
                endpoint = ensureTrailingSlashRemoved (endpoint);
                if (endpoint.isNotEmpty() && ! endpoints.contains (endpoint))
                    endpoints.add (endpoint);
            };

            auto base = ensureTrailingSlashRemoved (ensureHttpsScheme (configuredEndpoint));
            if (base.isEmpty())
                return endpoints;

            if (base.containsIgnoreCase ("plugin.club/api"))
            {
                add (PluginClubApi::sellerImportUrl());
                return endpoints;
            }

            const auto functionsBase = [&base]() -> juce::String
            {
                const auto stripped = stripKnownSellerSuffix (base);
                if (stripped.equalsIgnoreCase ("https://plugin.club")
                    || stripped.equalsIgnoreCase ("https://www.plugin.club")
                    || stripped.equalsIgnoreCase ("https://plugin.club/functions")
                    || stripped.equalsIgnoreCase ("https://www.plugin.club/functions")
                    || stripped.equalsIgnoreCase ("https://plugin.club/functions/v1")
                    || stripped.equalsIgnoreCase ("https://www.plugin.club/functions/v1"))
                    return kPluginClubDefaultFunctionsEndpoint;

                if (stripped.endsWithIgnoreCase ("/functions"))
                    return ensureTrailingSlashRemoved (stripped) + "/v1";

                if (stripped.containsIgnoreCase ("/functions/v1"))
                    return stripped;

                if (stripped.containsIgnoreCase ("/functions"))
                    return ensurePluginClubFunctionsPrefix (stripped);

                return ensurePluginClubFunctionsPrefix (stripped + "/functions/v1");
            }();

            add (ensureTrailingSlashRemoved (functionsBase) + "/" + kPluginClubSellerImportPath);

            if (base.containsIgnoreCase ("/functions/")
                && (base.endsWithIgnoreCase ("/" + juce::String (kPluginClubSellerImportPath))
                    || base.endsWithIgnoreCase ("/" + juce::String (kLegacyRomplurSellerImportPath))))
            {
                // Migrate romplurSellerImport (404 on live) to sellerImport.
                add (ensurePluginClubFunctionsPrefix (
                    stripKnownSellerSuffix (base) + "/" + kPluginClubSellerImportPath));
            }

            add (PluginClubApi::sellerImportUrl());
            return endpoints;
        }

        static juce::String resolvePluginClubBearerToken (const AiAssistService::CloudIntegrationConfig& config)
        {
            auto token = config.pluginClubAccessToken.trim();
            if (token.isEmpty())
                token = config.pluginClubApiKey.trim();
            if (token.isNotEmpty())
                return token;

           #if JUCE_WINDOWS
            char* envValue = nullptr;
            size_t envLen = 0;
            if (_dupenv_s (&envValue, &envLen, "PLUGINCLUB_API_KEY") == 0 && envValue != nullptr)
            {
                token = juce::String::fromUTF8 (envValue).trim();
                std::free (envValue);
            }
           #else
            if (const auto* envValue = std::getenv ("PLUGINCLUB_API_KEY"))
                token = juce::String::fromUTF8 (envValue).trim();
           #endif
            return token;
        }

        static juce::String pluginTypeForEngine (juce::String engine)
        {
            engine = engine.toLowerCase();
            if (engine.contains ("fx") || engine.contains ("effect"))
                return "fx";
            if (engine.contains ("sample") || engine.contains ("sampler") || engine.contains ("drum"))
                return "sampler";
            if (engine.contains ("utility"))
                return "utility";
            return "synth";
        }

        static juce::String artifactKindToStringLocal (PluginClubPublisher::ArtifactKind kind)
        {
            switch (kind)
            {
                case PluginClubPublisher::ArtifactKind::PatchCraftInstrumentPack: return "patchcraft_instrument_pack";
                case PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin:     return "standalone_vst3_plugin";
                case PluginClubPublisher::ArtifactKind::OneShotPack:              return "one_shot_pack";
                case PluginClubPublisher::ArtifactKind::LoopPack:                 return "loop_pack";
                case PluginClubPublisher::ArtifactKind::ControlAssetPack:         return "control_asset_pack";
                case PluginClubPublisher::ArtifactKind::GenericArchive:           return "generic_archive";
            }
            return "generic_archive";
        }

        static juce::String productTypeForArtifactKindLocal (PluginClubPublisher::ArtifactKind kind)
        {
            switch (kind)
            {
                case PluginClubPublisher::ArtifactKind::PatchCraftInstrumentPack: return "instrument";
                case PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin:     return "plugin";
                case PluginClubPublisher::ArtifactKind::OneShotPack:              return "sample_pack";
                case PluginClubPublisher::ArtifactKind::LoopPack:                 return "loop_pack";
                case PluginClubPublisher::ArtifactKind::ControlAssetPack:         return "asset_pack";
                case PluginClubPublisher::ArtifactKind::GenericArchive:           return "digital_product";
            }
            return "digital_product";
        }

        static juce::var stringArrayToVar (const juce::StringArray& values)
        {
            juce::Array<juce::var> array;
            for (const auto& value : values)
                array.add (value);
            return juce::var (array);
        }

        static juce::var buildPluginClubMetadata (const PatchCraftProject& project,
                                                  const juce::File& archiveFile)
        {
            const auto& manifest = project.getManifest();
            const auto title = manifest.instrumentName.isNotEmpty()
                ? manifest.instrumentName
                : juce::String ("PatchCraft Instrument");
            const auto description = manifest.description.isNotEmpty()
                ? manifest.description
                : juce::String ("Playable PatchCraft instrument package.");

            auto tags = manifest.tags;
            tags.addIfNotAlreadyThere ("PatchCraft");
            tags.addIfNotAlreadyThere (manifest.engine.isNotEmpty() ? manifest.engine : project.getEngineType());
            tags.addIfNotAlreadyThere (manifest.category);

            juce::StringArray formats;
            formats.add ("PatchCraft");

            juce::StringArray os;
            os.add ("Windows");
            os.add ("macOS");

            juce::StringArray daws;
            daws.add ("Ableton Live");
            daws.add ("FL Studio");
            daws.add ("Logic Pro");
            daws.add ("Pro Tools");
            daws.add ("Cubase");
            daws.add ("Studio One");
            daws.add ("Reaper");
            daws.add ("Bitwig");

            auto* compatibility = new juce::DynamicObject();
            compatibility->setProperty ("os", stringArrayToVar (os));
            compatibility->setProperty ("daws", stringArrayToVar (daws));
            compatibility->setProperty ("min_requirements", "PatchCraft Player compatible host.");

            auto* licenseConfig = new juce::DynamicObject();
            licenseConfig->setProperty ("license_type", manifest.licenseRequired ? "licensed" : "perpetual");
            licenseConfig->setProperty ("max_activations", 3);
            licenseConfig->setProperty ("require_hardware_validation", manifest.licenseBindToMachine);
            licenseConfig->setProperty ("allow_offline_validation", manifest.licenseOfflineGraceDays > 0);
            licenseConfig->setProperty ("trial_days", manifest.trialDays);

            auto* patchcraft = new juce::DynamicObject();
            patchcraft->setProperty ("package_kind", "patchcraft_pack");
            patchcraft->setProperty ("engine", manifest.engine.isNotEmpty() ? manifest.engine : project.getEngineType());
            patchcraft->setProperty ("category", manifest.category);
            patchcraft->setProperty ("creator", manifest.creator);
            patchcraft->setProperty ("archive_file", archiveFile.getFileName());
            patchcraft->setProperty ("pack_version", manifest.version);
            patchcraft->setProperty ("license_required", manifest.licenseRequired);
            patchcraft->setProperty ("license_product_id", manifest.licenseProductId);

            auto* sales = new juce::DynamicObject();
            sales->setProperty ("headline", manifest.salesHeadline);
            sales->setProperty ("subheadline", manifest.salesSubheadline);
            sales->setProperty ("cta", manifest.salesCtaText);
            sales->setProperty ("checkout_url", manifest.salesCheckoutUrl);
            sales->setProperty ("audio_demo_url", manifest.salesAudioDemoUrl);
            sales->setProperty ("video_demo_url", manifest.salesDemoVideoUrl);
            sales->setProperty ("currency", manifest.salesCurrency);
            sales->setProperty ("price", manifest.salesPrice);
            sales->setProperty ("compare_at_price", manifest.salesCompareAtPrice);
            sales->setProperty ("highlights", stringArrayToVar (manifest.salesHighlights));
            sales->setProperty ("includes", stringArrayToVar (manifest.salesIncludes));
            sales->setProperty ("faq", stringArrayToVar (manifest.salesFaq));

            auto* metadata = new juce::DynamicObject();
            metadata->setProperty ("title", title);
            metadata->setProperty ("source_product_id", manifest.licenseProductId.isNotEmpty()
                                                       ? safeSlug (manifest.licenseProductId)
                                                       : safeSlug (title));
            metadata->setProperty ("product_type", "instrument");
            metadata->setProperty ("plugin_type", pluginTypeForEngine (manifest.engine.isNotEmpty() ? manifest.engine : project.getEngineType()));
            metadata->setProperty ("plugin_format", stringArrayToVar (formats));
            metadata->setProperty ("price", manifest.salesPrice);
            metadata->setProperty ("currency", manifest.salesCurrency);
            metadata->setProperty ("checkout_url", manifest.salesCheckoutUrl);
            metadata->setProperty ("short_description", description.substring (0, 160));
            metadata->setProperty ("description", description);
            metadata->setProperty ("compatibility", juce::var (compatibility));
            metadata->setProperty ("version", manifest.version.isNotEmpty() ? manifest.version : juce::String ("1.0.0"));
            metadata->setProperty ("tags", stringArrayToVar (tags));
            metadata->setProperty ("status", "draft");
            metadata->setProperty ("artifact_kind", "patchcraft_instrument_pack");
            metadata->setProperty ("product_subtype", "patchcraft_pack");
            metadata->setProperty ("license_config", juce::var (licenseConfig));
            metadata->setProperty ("patchcraft", juce::var (patchcraft));
            metadata->setProperty ("sales_page", juce::var (sales));
            return juce::var (metadata);
        }

        static juce::var buildArtifactMetadata (const PluginClubPublisher::PublishArtifact& artifact,
                                                const juce::File& archiveFile)
        {
            const auto kind = artifact.kind;
            const auto title = artifact.title.trim().isNotEmpty()
                ? artifact.title.trim()
                : artifact.sourcePath.getFileNameWithoutExtension();
            const auto description = artifact.description.trim().isNotEmpty()
                ? artifact.description.trim()
                : juce::String ("Commercial product package prepared by PatchCraft Studio.");

            auto tags = artifact.tags;
            tags.addIfNotAlreadyThere ("PatchCraft");
            tags.addIfNotAlreadyThere (artifactKindToStringLocal (kind));
            if (artifact.category.isNotEmpty())
                tags.addIfNotAlreadyThere (artifact.category);

            auto formats = artifact.formats;
            if (formats.isEmpty())
            {
                if (kind == PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin)
                    formats.add ("VST3");
                else if (kind == PluginClubPublisher::ArtifactKind::OneShotPack
                         || kind == PluginClubPublisher::ArtifactKind::LoopPack)
                    formats.add ("WAV");
                else if (kind == PluginClubPublisher::ArtifactKind::ControlAssetPack)
                    formats.add ("PNG Filmstrip");
                else
                    formats.add ("PatchCraft");
            }

            auto os = artifact.operatingSystems;
            if (os.isEmpty())
            {
               #if JUCE_WINDOWS
                os.add ("Windows");
               #elif JUCE_MAC
                os.add ("macOS");
               #else
                os.add ("Cross-platform");
               #endif
            }

            auto daws = artifact.daws;
            if (daws.isEmpty())
            {
                daws.add ("Ableton Live");
                daws.add ("FL Studio");
                daws.add ("Logic Pro");
                daws.add ("Cubase");
                daws.add ("Studio One");
                daws.add ("Reaper");
                daws.add ("Bitwig");
            }

            auto* compatibility = new juce::DynamicObject();
            compatibility->setProperty ("os", stringArrayToVar (os));
            compatibility->setProperty ("daws", stringArrayToVar (daws));
            compatibility->setProperty ("min_requirements",
                                        kind == PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin
                                            ? "64-bit VST3 host."
                                            : "Standard ZIP extraction and compatible sampler/player.");

            auto* package = new juce::DynamicObject();
            package->setProperty ("artifact_kind", artifactKindToStringLocal (kind));
            package->setProperty ("source_path", artifact.sourcePath.getFullPathName());
            package->setProperty ("archive_file", archiveFile.getFileName());
            package->setProperty ("artwork_file", artifact.artworkFile.existsAsFile() ? artifact.artworkFile.getFileName() : juce::String());

            auto* metadata = new juce::DynamicObject();
            metadata->setProperty ("title", title);
            metadata->setProperty ("source_product_id", safeSlug (title));
            metadata->setProperty ("product_type", productTypeForArtifactKindLocal (kind));
            metadata->setProperty ("product_subtype", artifactKindToStringLocal (kind));
            metadata->setProperty ("artifact_kind", artifactKindToStringLocal (kind));
            metadata->setProperty ("plugin_type", kind == PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin ? "instrument" : "");
            metadata->setProperty ("plugin_format", stringArrayToVar (formats));
            metadata->setProperty ("price", artifact.price);
            metadata->setProperty ("short_description", description.substring (0, 160));
            metadata->setProperty ("description", description);
            metadata->setProperty ("creator", artifact.creator);
            metadata->setProperty ("category", artifact.category);
            metadata->setProperty ("compatibility", juce::var (compatibility));
            metadata->setProperty ("version", artifact.version.isNotEmpty() ? artifact.version : juce::String ("1.0.0"));
            metadata->setProperty ("tags", stringArrayToVar (tags));
            metadata->setProperty ("status", artifact.status.isNotEmpty() ? artifact.status : juce::String ("draft"));
            metadata->setProperty ("package", juce::var (package));
            if (! artifact.extraMetadata.isVoid())
                metadata->setProperty ("extra", artifact.extraMetadata);
            return juce::var (metadata);
        }

        static juce::var buildPublishPayload (const PatchCraftProject& project,
                                              const juce::File& packFolder,
                                              const juce::File& archiveFile,
                                              const juce::var& pluginClubMetadata)
        {
            const auto& manifest = project.getManifest();

            auto* license = new juce::DynamicObject();
            license->setProperty ("required", manifest.licenseRequired);
            license->setProperty ("productId", manifest.licenseProductId);
            license->setProperty ("serverUrl", manifest.licenseServerUrl);
            license->setProperty ("policy", manifest.licensePolicy);
            license->setProperty ("trialDays", manifest.trialDays);
            license->setProperty ("offlineGraceDays", manifest.licenseOfflineGraceDays);
            license->setProperty ("bindToMachine", manifest.licenseBindToMachine);
            license->setProperty ("instrumentId",
                                  LicenseValidator::hashInstrumentId (manifest.instrumentName,
                                                                       manifest.creator));

            auto* object = new juce::DynamicObject();
            object->setProperty ("type", "patchcraft.pack.publishDraft");
            object->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));
            object->setProperty ("packFolder", packFolder.getFullPathName());
            object->setProperty ("archiveFile", archiveFile.getFullPathName());
            object->setProperty ("manifest", manifest.toVar());
            object->setProperty ("license", juce::var (license));
            object->setProperty ("pluginClub", pluginClubMetadata);
            object->setProperty ("note",
                                 "Plugin.club seller import payload. The upload request sends multipart/form-data with package=<zip> and metadata=<JSON> to /functions/v1/sellerImport.");
            return juce::var (object);
        }

        static juce::var buildArtifactPayload (const PluginClubPublisher::PublishArtifact& artifact,
                                               const juce::File& stagingFolder,
                                               const juce::File& archiveFile,
                                               const juce::var& pluginClubMetadata)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty ("type", "patchcraft.artifact.publishDraft");
            object->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));
            object->setProperty ("artifactKind", artifactKindToStringLocal (artifact.kind));
            object->setProperty ("sourcePath", artifact.sourcePath.getFullPathName());
            object->setProperty ("stagingFolder", stagingFolder.getFullPathName());
            object->setProperty ("archiveFile", archiveFile.getFullPathName());
            object->setProperty ("pluginClub", pluginClubMetadata);
            object->setProperty ("note",
                                 "Plugin.club seller import payload. The upload request sends multipart/form-data with package=<zip> and metadata=<JSON> to /functions/v1/sellerImport.");
            return juce::var (object);
        }

        static bool addFolderToZip (juce::ZipFile::Builder& builder,
                                    const juce::File& folder,
                                    juce::String& error)
        {
            if (! folder.isDirectory())
            {
                error = "Pack folder is not a directory: " + folder.getFullPathName();
                return false;
            }

            juce::Array<juce::File> files;
            folder.findChildFiles (files, juce::File::findFiles, true);
            if (files.isEmpty())
            {
                error = "Pack folder is empty: " + folder.getFullPathName();
                return false;
            }

            const auto rootName = folder.getFileName();
            for (const auto& file : files)
            {
                auto relative = file.getRelativePathFrom (folder)
                    .replaceCharacter ('\\', '/')
                    .trimCharactersAtStart ("/");
                if (relative.isEmpty())
                    relative = file.getFileName();

                builder.addFile (file, 6, rootName + "/" + relative);
            }

            return true;
        }

        static bool addFileToZip (juce::ZipFile::Builder& builder,
                                  const juce::File& file,
                                  juce::String& error)
        {
            if (! file.existsAsFile())
            {
                error = "Package file does not exist: " + file.getFullPathName();
                return false;
            }

            builder.addFile (file, 6, file.getFileName());
            return true;
        }

        static bool writeZipArchive (const juce::File& folder,
                                     const juce::File& archiveFile,
                                     juce::String& error)
        {
            archiveFile.deleteFile();
            if (! archiveFile.getParentDirectory().createDirectory())
            {
                error = "Could not create archive folder: " + archiveFile.getParentDirectory().getFullPathName();
                return false;
            }

            juce::ZipFile::Builder builder;
            if (! addFolderToZip (builder, folder, error))
                return false;

            auto output = archiveFile.createOutputStream();
            if (output == nullptr || output->failedToOpen())
            {
                error = "Could not create Plugin.club archive: " + archiveFile.getFullPathName();
                return false;
            }

            double progress = 0.0;
            if (! builder.writeToStream (*output, &progress))
            {
                error = "Could not write Plugin.club archive.";
                return false;
            }

            output->flush();
            return archiveFile.existsAsFile() && archiveFile.getSize() > 0;
        }

        static bool writeSourceToZipArchive (const juce::File& sourcePath,
                                             const juce::File& archiveFile,
                                             juce::String& error)
        {
            archiveFile.deleteFile();
            if (! archiveFile.getParentDirectory().createDirectory())
            {
                error = "Could not create archive folder: " + archiveFile.getParentDirectory().getFullPathName();
                return false;
            }

            juce::ZipFile::Builder builder;
            if (sourcePath.isDirectory())
            {
                if (! addFolderToZip (builder, sourcePath, error))
                    return false;
            }
            else if (! addFileToZip (builder, sourcePath, error))
            {
                return false;
            }

            auto output = archiveFile.createOutputStream();
            if (output == nullptr || output->failedToOpen())
            {
                error = "Could not create Plugin.club archive: " + archiveFile.getFullPathName();
                return false;
            }

            double progress = 0.0;
            if (! builder.writeToStream (*output, &progress))
            {
                error = "Could not write Plugin.club archive.";
                return false;
            }

            output->flush();
            return archiveFile.existsAsFile() && archiveFile.getSize() > 0;
        }

        static juce::String redactedResponseSnippet (juce::String response)
        {
            response = response.replace ("\r", " ").replace ("\n", " ");
            return response.substring (0, 360);
        }

        static bool isTransientUploadFailure (int statusCode, const juce::String& response)
        {
            return statusCode == 0
                || statusCode == 408
                || statusCode == 429
                || statusCode >= 500
                || response.containsIgnoreCase ("bad gateway")
                || response.containsIgnoreCase ("deployment failed")
                || response.containsIgnoreCase ("isolate_internal_failure");
        }

        static juce::String publishFailureHint (int statusCode, const juce::String& response)
        {
            if (statusCode == 405 || response.containsIgnoreCase ("method not allowed"))
                return "\n\nPlugin.club backend functions must use the /functions/v1 prefix. "
                       "Use https://plugin.club/functions/v1/sellerImport in Settings.";

            if (statusCode >= 500 || response.containsIgnoreCase ("isolate_internal_failure"))
                return "\n\nThis is a Plugin.club backend/server failure after PatchCraft prepared the package. "
                       "The local archive and payload were kept so the same draft can be retried without rebuilding.";

            if (statusCode == 401 || statusCode == 403)
                return "\n\nSign in to Plugin.club in Settings (Device Auth) so Studio can send "
                       "Authorization: Bearer <access_token>. Seller profile required for publish.";

            return {};
        }

        static juce::String jsonStringProperty (const juce::DynamicObject* object, const char* name)
        {
            if (object == nullptr || ! object->hasProperty (name))
                return {};

            return object->getProperty (name).toString().trim();
        }

        static juce::String firstJsonStringProperty (const juce::DynamicObject* object,
                                                     std::initializer_list<const char*> names)
        {
            for (const auto* name : names)
            {
                auto value = jsonStringProperty (object, name);
                if (value.isNotEmpty())
                    return value;
            }

            return {};
        }

        static void readPublishResponseFields (const juce::DynamicObject* object,
                                               PluginClubPublisher::PublishResult& result)
        {
            result.draftId = firstJsonStringProperty (object, { "draft_id", "draftId" });
            result.sellerProductId = firstJsonStringProperty (object, { "seller_product_id", "sellerProductId", "product_id", "productId" });
            result.editUrl = firstJsonStringProperty (object, { "edit_url", "editUrl", "url" });
            result.errorCode = firstJsonStringProperty (object, { "code", "error_code", "errorCode" });
        }

        static juce::String publishResponseFailureMessage (const juce::DynamicObject* object,
                                                           int statusCode,
                                                           const juce::String& endpoint,
                                                           const juce::String& response)
        {
            const auto serverMessage = firstJsonStringProperty (object, { "error", "message" });
            const auto code = firstJsonStringProperty (object, { "code", "error_code", "errorCode" });

            juce::String message = "Plugin.club publish failed";
            if (statusCode > 0)
                message += " (HTTP " + juce::String (statusCode) + ")";
            if (endpoint.isNotEmpty())
                message += " from " + endpoint;
            if (code.isNotEmpty())
                message += "\nCode: " + code;
            if (serverMessage.isNotEmpty())
                message += "\nError: " + serverMessage;
            else if (response.isNotEmpty())
                message += "\nResponse: " + redactedResponseSnippet (response);

            return message;
        }

        static juce::String buildAuthHeaders (juce::String token)
        {
            token = token.trim();
            if (token.isEmpty())
                return "Accept: application/json\r\n";
            return "Accept: application/json\r\nAuthorization: Bearer " + token + "\r\n";
        }

        static bool uploadPackage (const PluginClubPublisher::PublishOptions& options,
                                   const juce::var& metadata,
                                   const juce::File& archiveFile,
                                   PluginClubPublisher::PublishResult& result)
        {
            if (options.endpoint.trim().isEmpty() || options.apiKey.trim().isEmpty())
            {
                result.success = true;
                result.uploaded = false;
                result.message = "Prepared Plugin.club package locally. Configure a Plugin.club endpoint/API key in Settings to push the draft online.";
                return true;
            }

            const auto endpoints = candidateEndpointsFor (options.endpoint);
            if (endpoints.isEmpty())
            {
                result.success = true;
                result.uploaded = false;
                result.message = "Prepared Plugin.club package locally. No valid Plugin.club endpoint was configured.";
                return true;
            }

            juce::String lastResponse;
            int attempts = 0;
            for (const auto& endpoint : endpoints)
            {
                constexpr int maxAttemptsPerEndpoint = 3;
                for (int attempt = 1; attempt <= maxAttemptsPerEndpoint; ++attempt)
                {
                    ++attempts;
                    int statusCode = 0;
                    juce::StringPairArray responseHeaders;
                    auto stream = juce::URL (endpoint)
                        .withParameter ("metadata", juce::JSON::toString (metadata, false))
                        .withParameter ("artifact_kind", artifactKindToStringLocal (result.artifactKind))
                        .withFileToUpload ("package", archiveFile, "application/zip")
                        .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                            .withConnectionTimeoutMs (juce::jlimit (3000, 120000, options.timeoutMs))
                            .withExtraHeaders (buildAuthHeaders (options.apiKey))
                            .withResponseHeaders (&responseHeaders)
                            .withStatusCode (&statusCode)
                            .withHttpRequestCmd ("POST")
                            .withNumRedirectsToFollow (2));

                    result.endpointUsed = endpoint;
                    result.statusCode = statusCode;

                    if (stream == nullptr)
                    {
                        lastResponse = "Could not connect.";
                        if (attempt < maxAttemptsPerEndpoint)
                            juce::Thread::sleep (450 * attempt);
                        continue;
                    }

                    const auto response = stream->readEntireStreamAsString();
                    lastResponse = response;
                    result.serverResponse = response;

                    const auto parsed = juce::JSON::parse (response);
                    if (auto* object = parsed.getDynamicObject())
                    {
                        readPublishResponseFields (object, result);

                        if (object->hasProperty ("ok") && ! (bool) object->getProperty ("ok"))
                        {
                            result.success = false;
                            result.uploaded = false;
                            result.message = publishResponseFailureMessage (object, statusCode, endpoint, response)
                                + publishFailureHint (statusCode, response);
                            return false;
                        }

                        if (statusCode >= 200 && statusCode < 300 && ! looksLikeHtmlResponse (response))
                        {
                            result.success = true;
                            result.uploaded = true;
                            result.message = "Plugin.club draft pushed successfully.";
                            if (result.draftId.isNotEmpty())
                                result.message += "\nDraft ID: " + result.draftId;
                            if (result.sellerProductId.isNotEmpty())
                                result.message += "\nProduct ID: " + result.sellerProductId;
                            if (result.editUrl.isNotEmpty())
                                result.message += "\nEdit URL: " + result.editUrl;
                            return true;
                        }

                        if ((statusCode > 0 && statusCode < 200) || statusCode >= 300)
                            lastResponse = publishResponseFailureMessage (object, statusCode, endpoint, response);
                    }

                    if (statusCode >= 200 && statusCode < 300 && ! looksLikeHtmlResponse (response))
                    {
                        result.success = true;
                        result.uploaded = true;
                        result.message = "Plugin.club draft pushed successfully.";
                        return true;
                    }

                    if (! isTransientUploadFailure (statusCode, response) || attempt >= maxAttemptsPerEndpoint)
                        break;

                    juce::Thread::sleep (450 * attempt);
                }
            }

            result.success = false;
            result.uploaded = false;
            result.message = "Prepared package locally, but Plugin.club publish failed with HTTP "
                + juce::String (result.statusCode) + " from " + result.endpointUsed
                + " after " + juce::String (attempts) + " attempt" + (attempts == 1 ? "" : "s")
                + ": " + redactedResponseSnippet (lastResponse)
                + publishFailureHint (result.statusCode, lastResponse);
            return true;
        }
    }

    PluginClubPublisher::PublishOptions PluginClubPublisher::optionsFromCloudConfig (const AiAssistService::CloudIntegrationConfig& config)
    {
        PublishOptions options;
        options.endpoint = config.pluginClubEndpoint.trim().isNotEmpty()
            ? normaliseSellerImportEndpoint (config.pluginClubEndpoint)
            : juce::String (kPluginClubDefaultFunctionsEndpoint) + "/" + kPluginClubSellerImportPath;
        if (options.endpoint.isEmpty())
            options.endpoint = juce::String (kPluginClubDefaultFunctionsEndpoint) + "/" + kPluginClubSellerImportPath;
        options.apiKey = resolvePluginClubBearerToken (config);
        options.timeoutMs = config.cloudTimeoutMs;
        options.stagingRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("PluginClubPublish");
        return options;
    }

    juce::String PluginClubPublisher::normaliseSellerImportEndpoint (const juce::String& configuredEndpoint)
    {
        const auto candidates = candidateEndpointsFor (configuredEndpoint);
        return candidates.isEmpty() ? juce::String() : candidates[0];
    }

    PluginClubPublisher::PublishResult PluginClubPublisher::publishDraft (const PatchCraftProject& project,
                                                                          const PublishOptions& options)
    {
        PublishResult result;
        result.artifactKind = ArtifactKind::PatchCraftInstrumentPack;

        auto stagingRoot = options.stagingRoot != juce::File()
            ? options.stagingRoot
            : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PatchCraft").getChildFile ("PluginClubPublish");
        if (! stagingRoot.createDirectory())
        {
            result.message = "Could not create Plugin.club staging folder: " + stagingRoot.getFullPathName();
            return result;
        }

        const auto folderName = safeSlug (project.getManifest().instrumentName)
            + "-" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
        result.packFolder = stagingRoot.getChildFile (folderName + ".patchcraft");

        PatchCraftPackWriter writer;
        juce::String error;
        if (! writer.write (project, result.packFolder, error))
        {
            result.message = "Pack staging failed: " + error;
            return result;
        }

        result.archiveFile = stagingRoot.getChildFile (folderName + ".zip");
        if (! writeZipArchive (result.packFolder, result.archiveFile, error))
        {
            result.message = "Pack archive failed: " + error;
            return result;
        }

        const auto metadata = buildPluginClubMetadata (project, result.archiveFile);
        result.metadataFile = result.packFolder.getChildFile ("pluginclub-metadata.json");
        if (! result.metadataFile.replaceWithText (juce::JSON::toString (metadata, true)))
        {
            result.message = "Could not write Plugin.club metadata.";
            return result;
        }

        const auto payload = buildPublishPayload (project, result.packFolder, result.archiveFile, metadata);
        result.payloadFile = result.packFolder.getChildFile ("pluginclub-publish.json");
        if (! result.payloadFile.replaceWithText (juce::JSON::toString (payload, true)))
        {
            result.message = "Could not write Plugin.club publish payload.";
            return result;
        }

        uploadPackage (options, metadata, result.archiveFile, result);
        return result;
    }

    PluginClubPublisher::PublishResult PluginClubPublisher::publishArtifact (const PublishArtifact& artifact,
                                                                             const PublishOptions& options)
    {
        PublishResult result;
        result.artifactKind = artifact.kind;

        if (artifact.sourcePath == juce::File() || ! artifact.sourcePath.exists())
        {
            result.message = "Publish artifact source does not exist: " + artifact.sourcePath.getFullPathName();
            return result;
        }

        auto stagingRoot = options.stagingRoot != juce::File()
            ? options.stagingRoot
            : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PatchCraft").getChildFile ("PluginClubPublish");
        if (! stagingRoot.createDirectory())
        {
            result.message = "Could not create Plugin.club staging folder: " + stagingRoot.getFullPathName();
            return result;
        }

        const auto title = artifact.title.trim().isNotEmpty()
            ? artifact.title.trim()
            : artifact.sourcePath.getFileNameWithoutExtension();
        const auto folderName = safeSlug (title) + "-"
            + artifactKindToStringLocal (artifact.kind) + "-"
            + juce::String (juce::Time::getCurrentTime().toMilliseconds());
        result.packFolder = stagingRoot.getChildFile (folderName + ".pluginclub");
        if (! result.packFolder.createDirectory())
        {
            result.message = "Could not create Plugin.club artifact staging folder: " + result.packFolder.getFullPathName();
            return result;
        }

        result.archiveFile = stagingRoot.getChildFile (folderName + ".zip");
        juce::String error;
        if (! writeSourceToZipArchive (artifact.sourcePath, result.archiveFile, error))
        {
            result.message = "Artifact archive failed: " + error;
            return result;
        }

        const auto metadata = buildArtifactMetadata (artifact, result.archiveFile);
        result.metadataFile = result.packFolder.getChildFile ("pluginclub-metadata.json");
        if (! result.metadataFile.replaceWithText (juce::JSON::toString (metadata, true)))
        {
            result.message = "Could not write Plugin.club metadata.";
            return result;
        }

        const auto payload = buildArtifactPayload (artifact, result.packFolder, result.archiveFile, metadata);
        result.payloadFile = result.packFolder.getChildFile ("pluginclub-publish.json");
        if (! result.payloadFile.replaceWithText (juce::JSON::toString (payload, true)))
        {
            result.message = "Could not write Plugin.club publish payload.";
            return result;
        }

        uploadPackage (options, metadata, result.archiveFile, result);
        return result;
    }

    juce::String PluginClubPublisher::artifactKindToString (ArtifactKind kind)
    {
        return artifactKindToStringLocal (kind);
    }

    juce::String PluginClubPublisher::productTypeForArtifactKind (ArtifactKind kind)
    {
        return productTypeForArtifactKindLocal (kind);
    }
}
