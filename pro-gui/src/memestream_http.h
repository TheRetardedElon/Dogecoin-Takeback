#pragma once

#include <string>
#include <vector>

/** One GPE Meme Stream card. tipAddress is the on-chain tip target. */
struct MemeItem {
    std::string id;
    std::string title;
    std::string body;
    std::string author;
    std::string tipAddress;
    std::string imageUrl;
    std::string createdAt;
    int likes = 0;
    double tipTotal = 0;
    bool featured = false;
};

struct MemeHttpReply {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};

/** Built-in Core publish key (obfuscated). Empty if unavailable. */
std::string MemePublishKey();

bool MemeUrlAllowed(const std::string& url);
std::string MemeResolveMediaUrl(const std::string& pathOrUrl);

MemeHttpReply MemeHttpGet(const std::string& url);
MemeHttpReply MemeHttpPostJson(const std::string& url, const std::string& json,
                               const std::vector<std::pair<std::string, std::string>>& headers);
MemeHttpReply MemeHttpPost(const std::string& url, const std::string& contentType,
                           const std::string& payload,
                           const std::vector<std::pair<std::string, std::string>>& headers);

std::string MemeGuessImageMime(const std::string& filename, const std::string& bytes);
std::string MemeBuildPublishMultipart(const std::string& title, const std::string& body,
                                      const std::string& wallet, const std::string& filename,
                                      const std::string& mime, const std::string& bytes,
                                      std::string& contentType);

bool MemeParseFeed(const std::string& json, std::vector<MemeItem>& items,
                   MemeItem* ofTheDay, std::string& err);
bool MemeParseItemObject(const std::string& obj, MemeItem& item);

std::string MemeJsonEscape(const std::string& s);
std::string MemeBuildPublishJson(const std::string& title, const std::string& body,
                                 const std::string& wallet);

inline constexpr const char* kMemeApiBase = "https://gopastearth.com";
inline constexpr const char* kMemeFeedPath = "/api/public/memestream/feed?limit=";
inline constexpr const char* kMemePublishPath = "/api/public/memestream/publish";
inline constexpr int kMemeMaxImageBytes = 70656;
