#pragma once

#include <string>

/** Core Pro identity for GPE arcade unlock (ImGui and Qt must match). */
namespace ArcadeHost {

inline constexpr const char* kHubUrl = "https://arcade.gopastearth.com/";
inline constexpr const char* kHubUrlLegacy = "https://gopastearth.com/arcade";
inline constexpr const char* kBuildTag = "1.14.103";
inline constexpr const char* kUaToken = "DogecoinCorePro/1.14.103";
inline constexpr const char* kHeaderName = "X-Dogecoin-Core-Pro";
inline constexpr const char* kHeaderValue = "1";
inline constexpr const char* kHeaderBuildName = "X-Dogecoin-Core-Pro-Build";

/**
 * Open the GPE cabinet with Core Pro identity so games unlock.
 *
 * Preferred: Edge/Chrome --app + User-Agent containing DogecoinCorePro/<ver>
 * (GPE accepts UA token; headers require full WebView2 embed).
 *
 * Returns true if a browser/app window was launched.
 */
bool LaunchCabinetUnlocked(const std::string& url = kHubUrl);

/** Full User-Agent string that includes the Core Pro token (GPE gate). */
std::string CoreProUserAgent();

} // namespace ArcadeHost
