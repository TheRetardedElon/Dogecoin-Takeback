#include "memestream_http.h"

#include "qt/memestreampublishkey.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>
#else
#include <fstream>
#endif

std::string MemePublishKey()
{
    if (!HasBuiltInMemeStreamPublishKey())
        return {};
    return GetBuiltInMemeStreamPublishKey();
}

std::string MemeJsonEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
            o += (char)c;
        } else if (c == '\n') {
            o += "\\n";
        } else if (c == '\r') {
            o += "\\r";
        } else if (c == '\t') {
            o += "\\t";
        } else if (c >= 0x20) {
            o += (char)c;
        }
    }
    return o;
}

std::string MemeBuildPublishJson(const std::string& title, const std::string& body,
                                 const std::string& wallet)
{
    std::ostringstream o;
    o << "{\"title\":\"" << MemeJsonEscape(title)
      << "\",\"body\":\"" << MemeJsonEscape(body)
      << "\",\"wallet\":\"" << MemeJsonEscape(wallet) << "\"}";
    return o.str();
}

static std::string ToLower(std::string s)
{
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

static bool HostAllowed(const std::string& host)
{
    const std::string h = ToLower(host);
    return h == "gopastearth.com" || h == "www.gopastearth.com" ||
           h == "memestream.gopastearth.com";
}

static bool PathAllowed(const std::string& path, bool mediaOk)
{
    if (path.find("..") != std::string::npos)
        return false;
    if (path.rfind("/api/public/memestream/", 0) == 0)
        return true;
    if (mediaOk && path.rfind("/media/memestream/", 0) == 0)
        return true;
    return false;
}

static bool SplitUrl(const std::string& url, std::string& scheme, std::string& host,
                     std::string& path, int& port)
{
    scheme.clear();
    host.clear();
    path = "/";
    port = 0;
    const char* p = url.c_str();
    if (std::strncmp(p, "https://", 8) == 0) {
        scheme = "https";
        p += 8;
        port = 443;
    } else if (std::strncmp(p, "http://", 7) == 0) {
        scheme = "http";
        p += 7;
        port = 80;
    } else {
        return false;
    }
    const char* slash = std::strchr(p, '/');
    const char* hostEnd = slash ? slash : p + std::strlen(p);
    host.assign(p, hostEnd);
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        port = std::atoi(host.c_str() + colon + 1);
        host.resize(colon);
    }
    if (slash)
        path = slash;
    return !host.empty();
}

bool MemeUrlAllowed(const std::string& url)
{
    std::string scheme, host, path;
    int port = 0;
    if (!SplitUrl(url, scheme, host, path, port))
        return false;
    if (scheme != "https")
        return false;
    auto q = path.find('?');
    const std::string pathOnly = (q == std::string::npos) ? path : path.substr(0, q);
    return HostAllowed(host) && PathAllowed(pathOnly, true);
}

std::string MemeResolveMediaUrl(const std::string& pathOrUrl)
{
    std::string s = pathOrUrl;
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    if (s.empty() || s == "null" || s == "undefined")
        return {};
    if (s.rfind("https://", 0) == 0 || s.rfind("http://", 0) == 0) {
        return MemeUrlAllowed(s) ? s : std::string();
    }
    if (s[0] != '/')
        s.insert(s.begin(), '/');
    if (s.rfind("/media/memestream/", 0) != 0)
        return {};
    return std::string(kMemeApiBase) + s;
}

#if defined(_WIN32)
// WinINet uses the same IE/Edge proxy + TLS stack as the working Site tab.
static void InetTimeouts(HINTERNET h)
{
    DWORD ms = 8000;
    InternetSetOptionA(h, INTERNET_OPTION_CONNECT_TIMEOUT, &ms, sizeof(ms));
    InternetSetOptionA(h, INTERNET_OPTION_SEND_TIMEOUT, &ms, sizeof(ms));
    InternetSetOptionA(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &ms, sizeof(ms));
}

static bool InetReadAll(HINTERNET req, std::string& body)
{
    char buf[4096];
    DWORD got = 0;
    while (InternetReadFile(req, buf, sizeof(buf), &got) && got > 0) {
        body.append(buf, got);
        if (body.size() > 2 * 1024 * 1024)
            break;
    }
    return !body.empty();
}

static MemeHttpReply WinInetDo(const char* method, const std::string& url,
                               const std::string& extraHeaders, const std::string& payload)
{
    MemeHttpReply r;
    std::string scheme, host, path;
    int port = 0;
    if (!SplitUrl(url, scheme, host, path, port)) {
        r.error = "Bad URL";
        return r;
    }
    HINTERNET ses = InternetOpenA("DogecoinCorePro/1.14.104",
                                  INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!ses) {
        r.error = "InternetOpen failed";
        return r;
    }
    InetTimeouts(ses);
    DWORD proto = 0x00000080 | 0x00000200; // TLS1.1 | TLS1.2
    InternetSetOptionA(ses, 84 /*INTERNET_OPTION_SECURE_PROTOCOLS*/, &proto, sizeof(proto));
    const DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                        INTERNET_FLAG_NO_UI | INTERNET_FLAG_KEEP_CONNECTION |
                        (scheme == "https" ? INTERNET_FLAG_SECURE : 0);
    HINTERNET req = nullptr;
    HINTERNET con = nullptr;
    if (std::strcmp(method, "GET") == 0 && payload.empty()) {
        req = InternetOpenUrlA(ses, url.c_str(),
                               extraHeaders.empty() ? nullptr : extraHeaders.c_str(),
                               extraHeaders.empty() ? 0 : (DWORD)extraHeaders.size(),
                               flags, 0);
        if (!req) {
            const DWORD e = GetLastError();
            r.error = "Could not reach " + host + " (Win32 " + std::to_string(e) + ")";
            InternetCloseHandle(ses);
            return r;
        }
    } else {
        con = InternetConnectA(ses, host.c_str(), (INTERNET_PORT)port,
                               nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
        if (!con) {
            r.error = "Could not connect to " + host;
            InternetCloseHandle(ses);
            return r;
        }
        InetTimeouts(con);
        req = HttpOpenRequestA(con, method, path.c_str(), nullptr, nullptr, nullptr, flags, 0);
        if (!req) {
            r.error = "OpenRequest failed";
            InternetCloseHandle(con);
            InternetCloseHandle(ses);
            return r;
        }
        std::string hdrs = extraHeaders;
        if (!HttpSendRequestA(req, hdrs.empty() ? nullptr : hdrs.c_str(),
                              hdrs.empty() ? 0 : (DWORD)hdrs.size(),
                              payload.empty() ? nullptr : (LPVOID)payload.data(),
                              (DWORD)payload.size())) {
            r.error = "HTTPS request failed";
            InternetCloseHandle(req);
            InternetCloseHandle(con);
            InternetCloseHandle(ses);
            return r;
        }
    }
    char statusBuf[16]{};
    DWORD statusLen = sizeof(statusBuf);
    DWORD idx = 0;
    if (HttpQueryInfoA(req, HTTP_QUERY_STATUS_CODE, statusBuf, &statusLen, &idx))
        r.status = std::atoi(statusBuf);
    InetReadAll(req, r.body);
    InternetCloseHandle(req);
    if (con)
        InternetCloseHandle(con);
    InternetCloseHandle(ses);
    if (r.status == 0 && !r.body.empty())
        r.status = 200;
    r.ok = r.status >= 200 && r.status < 300 && !r.body.empty();
    if (!r.ok && r.error.empty()) {
        if (r.status == 0)
            r.error = "Empty response";
        else
            r.error = "HTTP " + std::to_string(r.status);
    }
    return r;
}
#else
static std::string ShellQuote(const std::string& s)
{
    std::string o = "'";
    for (char c : s) {
        if (c == '\'')
            o += "'\\''";
        else
            o += c;
    }
    o += "'";
    return o;
}

static MemeHttpReply CurlDo(const char* method, const std::string& url,
                            const std::string& extraHeaders, const std::string& payload)
{
    MemeHttpReply r;
    std::string cmd = "curl -sS -L --max-time 25 -w '\\nHTTPSTATUS:%{http_code}' -X ";
    cmd += method;
    cmd += " -H 'Accept: application/json'";
    if (!extraHeaders.empty()) {
        std::string h = extraHeaders;
        size_t start = 0;
        while (start < h.size()) {
            size_t nl = h.find("\r\n", start);
            if (nl == std::string::npos)
                nl = h.size();
            std::string line = h.substr(start, nl - start);
            if (!line.empty()) {
                cmd += " -H ";
                cmd += ShellQuote(line);
            }
            start = (nl >= h.size()) ? h.size() : nl + 2;
        }
    }
    std::string tmp;
    if (!payload.empty()) {
        tmp = "/tmp/corepro-meme-post.json";
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            r.error = "Could not write temp POST body";
            return r;
        }
        f.write(payload.data(), (std::streamsize)payload.size());
        cmd += " --data-binary @";
        cmd += tmp;
    }
    cmd += " ";
    cmd += ShellQuote(url);
    cmd += " 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) {
        r.error = "curl not available";
        return r;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), p))
        r.body += buf;
    pclose(p);
    auto mark = r.body.rfind("HTTPSTATUS:");
    if (mark != std::string::npos) {
        r.status = std::atoi(r.body.c_str() + mark + 11);
        r.body.resize(mark);
        if (!r.body.empty() && r.body.back() == '\n')
            r.body.pop_back();
    }
    r.ok = r.status >= 200 && r.status < 300 && !r.body.empty();
    if (!r.ok && r.error.empty())
        r.error = r.status ? ("HTTP " + std::to_string(r.status)) : "Empty response";
    return r;
}
#endif

MemeHttpReply MemeHttpGet(const std::string& url)
{
    if (!MemeUrlAllowed(url)) {
        MemeHttpReply r;
        r.error = "URL not allowed";
        return r;
    }
#if defined(_WIN32)
    return WinInetDo("GET", url, "Accept: application/json\r\n", {});
#else
    return CurlDo("GET", url, {}, {});
#endif
}

MemeHttpReply MemeHttpPost(const std::string& url, const std::string& contentType,
                           const std::string& payload,
                           const std::vector<std::pair<std::string, std::string>>& headers)
{
    if (!MemeUrlAllowed(url)) {
        MemeHttpReply r;
        r.error = "URL not allowed";
        return r;
    }
    std::string extra = "Accept: application/json\r\n";
    if (!contentType.empty())
        extra += "Content-Type: " + contentType + "\r\n";
    for (const auto& h : headers)
        extra += h.first + ": " + h.second + "\r\n";
#if defined(_WIN32)
    return WinInetDo("POST", url, extra, payload);
#else
    return CurlDo("POST", url, extra, payload);
#endif
}

MemeHttpReply MemeHttpPostJson(const std::string& url, const std::string& json,
                               const std::vector<std::pair<std::string, std::string>>& headers)
{
    return MemeHttpPost(url, "application/json", json, headers);
}

static std::string BasenameOnly(const std::string& path)
{
    size_t sl = path.find_last_of("/\\");
    std::string n = (sl == std::string::npos) ? path : path.substr(sl + 1);
    std::string o;
    for (unsigned char c : n) {
        if (c == '"' || c == '\r' || c == '\n' || c == '\\')
            continue;
        o += (char)c;
    }
    return o.empty() ? "meme.bin" : o;
}

std::string MemeGuessImageMime(const std::string& filename, const std::string& bytes)
{
    auto lower = filename;
    for (char& c : lower)
        c = (char)std::tolower((unsigned char)c);
    if (bytes.size() >= 8 && (unsigned char)bytes[0] == 0x89 && bytes.compare(1, 3, "PNG") == 0)
        return "image/png";
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xff && (unsigned char)bytes[1] == 0xd8)
        return "image/jpeg";
    if (bytes.size() >= 6 &&
        (bytes.compare(0, 6, "GIF87a") == 0 || bytes.compare(0, 6, "GIF89a") == 0))
        return "image/gif";
    if (bytes.size() >= 12 && bytes.compare(0, 4, "RIFF") == 0 && bytes.compare(8, 4, "WEBP") == 0)
        return "image/webp";
    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".png") == 0)
        return "image/png";
    if (lower.size() >= 4 &&
        (lower.compare(lower.size() - 4, 4, ".jpg") == 0 ||
         (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".jpeg") == 0)))
        return "image/jpeg";
    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".gif") == 0)
        return "image/gif";
    if (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".webp") == 0)
        return "image/webp";
    return "application/octet-stream";
}

std::string MemeBuildPublishMultipart(const std::string& title, const std::string& body,
                                      const std::string& wallet, const std::string& filename,
                                      const std::string& mime, const std::string& bytes,
                                      std::string& contentType)
{
    char bound[48];
    std::snprintf(bound, sizeof(bound), "----CoreProMeme%08x%04x",
                  (unsigned)std::time(nullptr), (unsigned)(bytes.size() & 0xffff));
    contentType = std::string("multipart/form-data; boundary=") + bound;
    const std::string fname = BasenameOnly(filename);
    const std::string ctype = mime.empty() ? "application/octet-stream" : mime;
    std::string o;
    o.reserve(bytes.size() + 512);
    auto textPart = [&](const char* name, const std::string& val) {
        o += "--";
        o += bound;
        o += "\r\nContent-Disposition: form-data; name=\"";
        o += name;
        o += "\"\r\n\r\n";
        o += val;
        o += "\r\n";
    };
    textPart("title", title);
    textPart("body", body);
    textPart("wallet", wallet);
    o += "--";
    o += bound;
    o += "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"";
    o += fname;
    o += "\"\r\nContent-Type: ";
    o += ctype;
    o += "\r\n\r\n";
    o += bytes;
    o += "\r\n--";
    o += bound;
    o += "--\r\n";
    return o;
}

static void SkipWs(const std::string& s, size_t& i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
        ++i;
}

static bool ParseJsonString(const std::string& s, size_t& i, std::string& out)
{
    out.clear();
    if (i >= s.size() || s[i] != '"')
        return false;
    ++i;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"')
            return true;
        if (c == '\\' && i < s.size()) {
            char e = s[i++];
            if (e == 'n')
                out += '\n';
            else if (e == 'r')
                out += '\r';
            else if (e == 't')
                out += '\t';
            else
                out += e;
        } else {
            out += c;
        }
    }
    return false;
}

static bool FindKeyValue(const std::string& obj, const char* key, size_t& valuePos)
{
    const std::string pat = std::string("\"") + key + "\"";
    size_t i = 0;
    while (i < obj.size()) {
        size_t at = obj.find(pat, i);
        if (at == std::string::npos)
            return false;
        size_t j = at + pat.size();
        SkipWs(obj, j);
        if (j < obj.size() && obj[j] == ':') {
            ++j;
            SkipWs(obj, j);
            valuePos = j;
            return true;
        }
        i = at + 1;
    }
    return false;
}

static std::string JsonStringField(const std::string& obj, const char* key)
{
    size_t p = 0;
    if (!FindKeyValue(obj, key, p) || p >= obj.size())
        return {};
    if (obj.compare(p, 4, "null") == 0)
        return {};
    if (obj[p] != '"')
        return {};
    std::string v;
    ParseJsonString(obj, p, v);
    return v;
}

static int JsonIntField(const std::string& obj, const char* key, int def = 0)
{
    size_t p = 0;
    if (!FindKeyValue(obj, key, p) || p >= obj.size())
        return def;
    if (obj.compare(p, 4, "null") == 0)
        return def;
    return std::atoi(obj.c_str() + p);
}

static double JsonDoubleField(const std::string& obj, const char* key)
{
    size_t p = 0;
    if (!FindKeyValue(obj, key, p) || p >= obj.size())
        return 0;
    if (obj.compare(p, 4, "null") == 0)
        return 0;
    return std::atof(obj.c_str() + p);
}

static bool JsonBoolField(const std::string& obj, const char* key)
{
    size_t p = 0;
    if (!FindKeyValue(obj, key, p) || p >= obj.size())
        return false;
    return obj.compare(p, 4, "true") == 0;
}

static bool ExtractObjectAt(const std::string& s, size_t start, std::string& obj, size_t& end)
{
    SkipWs(s, start);
    if (start >= s.size() || s[start] != '{')
        return false;
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) {
            if (esc)
                esc = false;
            else if (c == '\\')
                esc = true;
            else if (c == '"')
                inStr = false;
            continue;
        }
        if (c == '"')
            inStr = true;
        else if (c == '{')
            depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) {
                obj = s.substr(start, i - start + 1);
                end = i + 1;
                return true;
            }
        }
    }
    return false;
}

static bool ExtractArrayObjects(const std::string& json, const char* key,
                                std::vector<std::string>& objs)
{
    size_t p = 0;
    if (!FindKeyValue(json, key, p))
        return false;
    SkipWs(json, p);
    if (p >= json.size() || json[p] != '[')
        return false;
    ++p;
    for (;;) {
        SkipWs(json, p);
        if (p >= json.size())
            return false;
        if (json[p] == ']')
            return true;
        if (json[p] == ',') {
            ++p;
            continue;
        }
        std::string obj;
        size_t end = 0;
        if (!ExtractObjectAt(json, p, obj, end))
            return false;
        objs.push_back(std::move(obj));
        p = end;
    }
}

bool MemeParseItemObject(const std::string& obj, MemeItem& item)
{
    item = MemeItem();
    item.id = JsonStringField(obj, "id");
    if (item.id.empty())
        item.id = JsonStringField(obj, "_id");
    item.title = JsonStringField(obj, "title");
    item.body = JsonStringField(obj, "body");
    if (item.body.empty())
        item.body = JsonStringField(obj, "caption");
    item.author = JsonStringField(obj, "author");
    if (item.author.empty())
        item.author = JsonStringField(obj, "wallet");
    item.tipAddress = JsonStringField(obj, "tipAddress");
    if (item.tipAddress.empty())
        item.tipAddress = item.author;
    item.imageUrl = JsonStringField(obj, "imageUrl");
    if (item.imageUrl.empty())
        item.imageUrl = JsonStringField(obj, "image");
    if (item.imageUrl.empty())
        item.imageUrl = JsonStringField(obj, "mediaUrl");
    item.imageUrl = MemeResolveMediaUrl(item.imageUrl);
    item.createdAt = JsonStringField(obj, "createdAt");
    if (item.createdAt.empty())
        item.createdAt = JsonStringField(obj, "created");
    item.likes = JsonIntField(obj, "likes", JsonIntField(obj, "likeCount", 0));
    item.tipTotal = JsonDoubleField(obj, "tipTotalDoge");
    item.featured = JsonBoolField(obj, "featured");
    return !item.id.empty() || !item.title.empty() || !item.body.empty();
}

bool MemeParseFeed(const std::string& json, std::vector<MemeItem>& items,
                   MemeItem* ofTheDay, std::string& err)
{
    items.clear();
    err.clear();
    if (json.size() >= 15 && (json[0] == '<' || json.compare(0, 9, "<!DOCTYPE") == 0 ||
                              json.compare(0, 5, "<html") == 0)) {
        err = "Feed returned HTML, not JSON";
        return false;
    }
    std::vector<std::string> objs;
    if (!ExtractArrayObjects(json, "items", objs)) {
        if (!ExtractArrayObjects(json, "memes", objs))
            ExtractArrayObjects(json, "data", objs);
    }
    if (objs.empty() && !json.empty() && json[0] == '[') {
        size_t p = 0;
        ExtractArrayObjects(std::string("{\"items\":") + json + "}", "items", objs);
        (void)p;
    }
    if (objs.empty()) {
        err = "Unexpected feed JSON shape";
        return false;
    }
    for (const auto& o : objs) {
        MemeItem it;
        if (MemeParseItemObject(o, it))
            items.push_back(std::move(it));
    }
    if (ofTheDay) {
        size_t p = 0;
        *ofTheDay = MemeItem();
        if (FindKeyValue(json, "dogeOfTheDay", p)) {
            std::string obj;
            size_t end = 0;
            if (ExtractObjectAt(json, p, obj, end))
                MemeParseItemObject(obj, *ofTheDay);
        }
    }
    return true;
}
