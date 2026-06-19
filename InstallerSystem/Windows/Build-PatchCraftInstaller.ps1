param(
    [ValidateSet("StudioStandalone", "PlayerInstrument")]
    [string] $Mode = "PlayerInstrument",

    [Parameter(Mandatory = $true)]
    [string] $ProductName,

    [string] $Publisher = "AudiCode",
    [string] $Version = "1.0.0",

    [string] $StandaloneExe = "",
    [string] $Vst3Bundle = "",
    [string] $PackFolder = "",
    [string] $IconPath = "",
    [string] $EulaPath = "",

    [string] $InstallerId = "",
    [ValidateSet("PerUser", "System")]
    [string] $Vst3Scope = "PerUser",
    [string] $Vst3InstallPath = "",

    [switch] $LicenseRequired,
    [switch] $RequireLicenseOnFirstRun,
    [string] $LicenseProductId = "",
    [string] $LicenseServerUrl = "",
    [int] $TrialDays = 0,
    [int] $OfflineGraceDays = 14,
    [bool] $BindLicenseToMachine = $true,
    [string] $SupportUrl = "",
    [string] $ManualUrl = "",
    [string] $StoreUrl = "",
    [string] $PrivacyUrl = "",

    [string] $OutputRoot = "",
    [switch] $Compile
)

$ErrorActionPreference = "Stop"

function Get-SafeName {
    param([string] $Text)
    $safe = ($Text -replace '[\\/:*?"<>|]', '_' -replace '\s+', '_').Trim('_')
    if ([string]::IsNullOrWhiteSpace($safe)) { return "PatchCraftProduct" }
    return $safe
}

function Resolve-RequiredPath {
    param(
        [string] $Path,
        [string] $Label
    )
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Label is required for mode $Mode."
    }
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    return $resolved.Path
}

function Copy-DirectoryPayload {
    param(
        [string] $Source,
        [string] $Destination
    )
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

function Find-InnoCompiler {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 5\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }

    $cmd = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return ""
}

function New-InnoString {
    param([string] $Text)
    return ($Text -replace '"', '""')
}

function New-StableGuidFromText {
    param([string] $Text)
    if ([string]::IsNullOrWhiteSpace($Text)) { $Text = "PatchCraftProduct" }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $hash = $sha.ComputeHash($bytes)
        $hex = -join ($hash | ForEach-Object { $_.ToString("x2") })
        return "{{{0}-{1}-{2}-{3}-{4}}}" -f $hex.Substring(0, 8), $hex.Substring(8, 4), $hex.Substring(12, 4), $hex.Substring(16, 4), $hex.Substring(20, 12)
    }
    finally {
        $sha.Dispose()
    }
}

function ConvertTo-InnoPath {
    param([string] $Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    $converted = $Path
    $converted = $converted -replace '%LOCALAPPDATA%', '{localappdata}'
    $converted = $converted -replace '%APPDATA%', '{userappdata}'
    $converted = $converted -replace 'CommonFilesFolder\\VST3', '{commoncf64}\VST3'
    $converted = $converted -replace 'CommonFilesFolder/VST3', '{commoncf64}\VST3'
    return $converted
}

function New-ChecksumManifest {
    param(
        [string] $Root,
        [string] $OutputFile
    )

    $files = @()
    if (Test-Path -LiteralPath $Root) {
        $rootFull = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        $files = Get-ChildItem -LiteralPath $Root -File -Recurse |
            Where-Object { $_.FullName -ne $OutputFile } |
            Sort-Object FullName |
            ForEach-Object {
                $relative = $_.FullName
                if ($relative.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
                    $relative = $relative.Substring($rootFull.Length)
                }
                $relative = $relative.Replace('\', '/')
                [ordered]@{
                    path = $relative
                    bytes = $_.Length
                    sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                    modified = $_.LastWriteTimeUtc.ToString("o")
                }
            }
    }

    $manifest = [ordered]@{
        schema = "patchcraft.installer_artifacts.v1"
        generated_at = (Get-Date).ToUniversalTime().ToString("o")
        file_count = @($files).Count
        files = $files
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputFile -Encoding UTF8
}

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')
$safeName = Get-SafeName $ProductName

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot "build-codex\dist\Installers"
}

$productRoot = Join-Path $OutputRoot $safeName
$payloadRoot = Join-Path $productRoot "payload"
$installerRoot = Join-Path $productRoot "installer"
$outputDir = Join-Path $installerRoot "Output"

New-Item -ItemType Directory -Force -Path $payloadRoot, $installerRoot, $outputDir | Out-Null

$manifest = [ordered]@{
    schema = "patchcraft.installer_manifest.v1"
    mode = $Mode
    product_name = $ProductName
    publisher = $Publisher
    version = $Version
    generated_at = (Get-Date).ToString("o")
    installer_id = if ([string]::IsNullOrWhiteSpace($InstallerId)) { New-StableGuidFromText "$Publisher|$ProductName|$Version" } else { $InstallerId }
    payload = [ordered]@{}
    install = [ordered]@{
        vst3_scope = $Vst3Scope
        vst3_path = $Vst3InstallPath
    }
    license = [ordered]@{
        required = [bool] $LicenseRequired
        require_on_first_run = [bool] $RequireLicenseOnFirstRun
        product_id = $LicenseProductId
        server_url = $LicenseServerUrl
        trial_days = $TrialDays
        offline_grace_days = $OfflineGraceDays
        bind_to_machine = [bool] $BindLicenseToMachine
    }
    support = [ordered]@{
        support_url = $SupportUrl
        manual_url = $ManualUrl
        store_url = $StoreUrl
        privacy_url = $PrivacyUrl
    }
}

if ($Mode -eq "StudioStandalone") {
    $exePath = Resolve-RequiredPath $StandaloneExe "StandaloneExe"
    $appDest = Join-Path $payloadRoot "app"
    New-Item -ItemType Directory -Force -Path $appDest | Out-Null
    Copy-Item -LiteralPath $exePath -Destination (Join-Path $appDest (Split-Path $exePath -Leaf)) -Force
    $manifest.payload.standalone_exe = (Split-Path $exePath -Leaf)
}
else {
    if (-not [string]::IsNullOrWhiteSpace($Vst3Bundle)) {
        $vst3Path = Resolve-RequiredPath $Vst3Bundle "Vst3Bundle"
        $vst3Name = Split-Path $vst3Path -Leaf
        $vst3Dest = Join-Path (Join-Path $payloadRoot "vst3") $vst3Name
        Copy-DirectoryPayload -Source $vst3Path -Destination $vst3Dest
        $manifest.payload.vst3_bundle = $vst3Name
    }

    if (-not [string]::IsNullOrWhiteSpace($PackFolder)) {
        $packPath = Resolve-RequiredPath $PackFolder "PackFolder"
        $packName = Split-Path $packPath -Leaf
        $packDest = Join-Path (Join-Path $payloadRoot "packs") $packName
        Copy-DirectoryPayload -Source $packPath -Destination $packDest
        $manifest.payload.pack_folder = $packName
    }

    if (-not $manifest.payload.Contains("vst3_bundle") -and -not $manifest.payload.Contains("pack_folder")) {
        throw "PlayerInstrument requires Vst3Bundle, PackFolder, or both."
    }
}

$whiteLabelProduct = [ordered]@{
    schema = "patchcraft.installer_white_label_product.v1"
    product = [ordered]@{
        name = $ProductName
        publisher = $Publisher
        version = $Version
        installer_id = $manifest.installer_id
    }
    payload = $manifest.payload
    install = $manifest.install
    license = $manifest.license
    support = $manifest.support
}

$activationRequest = [ordered]@{
    type = "patchcraft.license.activationRequest"
    productName = $ProductName
    productId = $LicenseProductId
    licenseServerUrl = $LicenseServerUrl
    trial = $TrialDays -gt 0
    trialDays = $TrialDays
    offlineGraceDays = $OfflineGraceDays
    bindToMachine = [bool] $BindLicenseToMachine
    machineId = "RUNTIME_MACHINE_ID"
    requestedAt = (Get-Date).ToUniversalTime().ToString("o")
}

$manifestPath = Join-Path $installerRoot "installer-manifest.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$whiteLabelPath = Join-Path $installerRoot "white-label-product.json"
$whiteLabelProduct | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $whiteLabelPath -Encoding UTF8
$activationPath = Join-Path $installerRoot "license-activation.json"
$activationRequest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $activationPath -Encoding UTF8

$appId = if ([string]::IsNullOrWhiteSpace($InstallerId)) { $manifest.installer_id } else { $InstallerId }
$escapedProduct = New-InnoString $ProductName
$escapedPublisher = New-InnoString $Publisher
$escapedVersion = New-InnoString $Version
$escapedProductRoot = New-InnoString $productRoot
$escapedOutputDir = New-InnoString $outputDir
$vst3Dest = if (-not [string]::IsNullOrWhiteSpace($Vst3InstallPath)) {
    ConvertTo-InnoPath $Vst3InstallPath
}
elseif ($Vst3Scope -eq "System") {
    "{commoncf64}\VST3"
}
else {
    "{localappdata}\Programs\Common\VST3"
}

$iss = New-Object System.Collections.Generic.List[string]
$iss.Add("; Generated by PatchCraft Installer System")
$iss.Add("#define ProductName `"$escapedProduct`"")
$iss.Add("#define ProductPublisher `"$escapedPublisher`"")
$iss.Add("#define ProductVersion `"$escapedVersion`"")
$iss.Add("#define ProductRoot `"$escapedProductRoot`"")
$iss.Add("")
$iss.Add("[Setup]")
$iss.Add("AppId=$appId")
$iss.Add("AppName={#ProductName}")
$iss.Add("AppVersion={#ProductVersion}")
$iss.Add("AppPublisher={#ProductPublisher}")
$iss.Add("DefaultDirName={localappdata}\Programs\{#ProductPublisher}\{#ProductName}")
$iss.Add("DefaultGroupName={#ProductName}")
$iss.Add("OutputDir=$escapedOutputDir")
$iss.Add("OutputBaseFilename=$safeName-Setup")
$iss.Add("Compression=lzma2")
$iss.Add("SolidCompression=yes")
$iss.Add("ArchitecturesAllowed=x64compatible")
$iss.Add("ArchitecturesInstallIn64BitMode=x64compatible")
$iss.Add("PrivilegesRequired=$(if ($Vst3Scope -eq 'System') { 'admin' } else { 'lowest' })")
$iss.Add("UninstallDisplayName={#ProductName}")
if (-not [string]::IsNullOrWhiteSpace($IconPath)) {
    $iconResolved = Resolve-RequiredPath $IconPath "IconPath"
    $iss.Add("SetupIconFile=$(New-InnoString $iconResolved)")
}
if (-not [string]::IsNullOrWhiteSpace($EulaPath)) {
    $eulaResolved = Resolve-RequiredPath $EulaPath "EulaPath"
    $iss.Add("LicenseFile=$(New-InnoString $eulaResolved)")
}
$iss.Add("")
$iss.Add("[Files]")
$iss.Add("Source: `"{#ProductRoot}\installer\installer-manifest.json`"; DestDir: `"{app}`"; Flags: ignoreversion")
$iss.Add("Source: `"{#ProductRoot}\installer\white-label-product.json`"; DestDir: `"{app}`"; Flags: ignoreversion")
$iss.Add("Source: `"{#ProductRoot}\installer\license-activation.json`"; DestDir: `"{app}`"; Flags: ignoreversion")
$iss.Add("Source: `"{#ProductRoot}\installer\artifact-manifest.json`"; DestDir: `"{app}`"; Flags: ignoreversion skipifsourcedoesntexist")

if ($Mode -eq "StudioStandalone") {
    $exeName = $manifest.payload.standalone_exe
    $iss.Add("Source: `"{#ProductRoot}\payload\app\*`"; DestDir: `"{app}`"; Flags: recursesubdirs ignoreversion")
    $iss.Add("")
    $iss.Add("[Icons]")
    $iss.Add("Name: `"{group}\{#ProductName}`"; Filename: `"{app}\$exeName`"")
    $iss.Add("Name: `"{userdesktop}\{#ProductName}`"; Filename: `"{app}\$exeName`"; Tasks: desktopicon")
    $iss.Add("")
    $iss.Add("[Tasks]")
    $iss.Add("Name: desktopicon; Description: `"Create a desktop shortcut`"; GroupDescription: `"Additional icons:`"; Flags: unchecked")
}
else {
    if ($manifest.payload.Contains("vst3_bundle")) {
        $vst3Name = $manifest.payload.vst3_bundle
        $iss.Add("Source: `"{#ProductRoot}\payload\vst3\$vst3Name\*`"; DestDir: `"$vst3Dest\$vst3Name`"; Flags: recursesubdirs ignoreversion")
    }
    if ($manifest.payload.Contains("pack_folder")) {
        $packName = $manifest.payload.pack_folder
        $iss.Add("Source: `"{#ProductRoot}\payload\packs\$packName\*`"; DestDir: `"{userappdata}\{#ProductPublisher}\{#ProductName}\Packs\$packName`"; Flags: recursesubdirs ignoreversion")
    }
}

$iss.Add("")
$iss.Add("[UninstallDelete]")
$iss.Add("; Buyer presets, imports, license cache, recordings, and user MIDI remain under AppData and are intentionally preserved.")

$issPath = Join-Path $installerRoot "$safeName.iss"
$iss | Set-Content -LiteralPath $issPath -Encoding UTF8

$artifactPath = Join-Path $installerRoot "artifact-manifest.json"
New-ChecksumManifest -Root $productRoot -OutputFile $artifactPath

Write-Host "Installer staging complete:"
Write-Host "  Manifest: $manifestPath"
Write-Host "  Product:  $whiteLabelPath"
Write-Host "  License:  $activationPath"
Write-Host "  Artifacts:$artifactPath"
Write-Host "  Inno:     $issPath"
Write-Host "  Output:   $outputDir"

if ($Compile) {
    $iscc = Find-InnoCompiler
    if ([string]::IsNullOrWhiteSpace($iscc)) {
        throw "Inno Setup compiler was not found. Install Inno Setup 6 or run without -Compile to review the .iss script."
    }

    & $iscc $issPath
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup failed with exit code $LASTEXITCODE."
    }
    Write-Host "Compiled installer output:"
    Get-ChildItem -LiteralPath $outputDir -Filter "*.exe" | ForEach-Object { Write-Host "  $($_.FullName)" }
}
