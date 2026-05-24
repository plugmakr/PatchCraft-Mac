# This script is invoked via CMake to build the AI Expansion addon package.
# It stages the required dynamic libraries (e.g., libfaust) and AI models into an installable payload.

message(STATUS "Packaging AI Expansion addon...")

set(AI_PAYLOAD_DIR "${CMAKE_BINARY_DIR}/PatchCraftAiExpansion/payload")
file(MAKE_DIRECTORY "${AI_PAYLOAD_DIR}")

# 1. We expect libfaust.dylib (macOS) or faust.dll (Windows) to be present in the source lib/ directory.
set(LIBFAUST_SRC "${CMAKE_SOURCE_DIR}/lib/libfaust.dylib")
if(EXISTS "${LIBFAUST_SRC}")
    message(STATUS "Copying libfaust.dylib into AI Expansion payload...")
    file(COPY "${LIBFAUST_SRC}" DESTINATION "${AI_PAYLOAD_DIR}/AiExpansion")
else()
    message(WARNING "libfaust.dylib not found in Source/lib! The AI Expansion will not be fully functional without it.")
endif()

# 2. In the future, we will also copy local LLM binaries (e.g. llama-cli) and GGUF models.

# 3. Create a manifest for the AI Expansion
set(MANIFEST_CONTENT "{ \"name\": \"PatchCraft AI Expansion\", \"version\": \"1.0.0\" }")
file(WRITE "${AI_PAYLOAD_DIR}/AiExpansion/manifest.json" "${MANIFEST_CONTENT}")

message(STATUS "PatchCraft AI Expansion packaging complete. Payload ready at: ${AI_PAYLOAD_DIR}")
