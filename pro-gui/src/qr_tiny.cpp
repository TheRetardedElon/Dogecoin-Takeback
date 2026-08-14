#include "qr_tiny.h"

#include <algorithm>
#include <cstring>

// Compact QR Code generator (byte mode, ECC-M, versions 1–10).
// Structure follows ISO/IEC 18004. Not a full library — enough for dogecoin: URIs.

static const int kCapM[] = {0, 14, 26, 42, 62, 84, 106, 122, 152, 180, 213};
static const int kEccM[] = {0, 10, 16, 26, 36, 48, 64, 72, 88, 110, 130};
static const int kCwM[]  = {0, 19, 34, 55, 80, 108, 136, 156, 194, 232, 274};
static const int kBlocksM[] = {0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5};

static const int kAlign[][8] = {
    {0},
    {0},
    {6, 18},
    {6, 22},
    {6, 26},
    {6, 30},
    {6, 34},
    {6, 22, 38},
    {6, 24, 42},
    {6, 26, 46},
    {6, 28, 50},
};

static uint8_t gfExp[512];
static uint8_t gfLog[256];
static bool gfReady = false;

static void GfInit()
{
    if (gfReady)
        return;
    int x = 1;
    for (int i = 0; i < 255; ++i) {
        gfExp[i] = (uint8_t)x;
        gfLog[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100)
            x ^= 0x11d;
    }
    for (int i = 255; i < 512; ++i)
        gfExp[i] = gfExp[i - 255];
    gfLog[0] = 0;
    gfReady = true;
}

static uint8_t GfMul(uint8_t a, uint8_t b)
{
    if (!a || !b)
        return 0;
    return gfExp[gfLog[a] + gfLog[b]];
}

static void RsGenerator(int deg, std::vector<uint8_t>& g)
{
    g.assign(deg + 1, 0);
    g[0] = 1;
    for (int i = 0; i < deg; ++i) {
        for (int j = i + 1; j > 0; --j)
            g[j] ^= GfMul(g[j - 1], gfExp[i]);
    }
}

static void RsEncode(const uint8_t* data, int n, int ecc, uint8_t* out)
{
    std::vector<uint8_t> gen;
    RsGenerator(ecc, gen);
    std::vector<uint8_t> msg(n + ecc, 0);
    std::memcpy(msg.data(), data, n);
    for (int i = 0; i < n; ++i) {
        uint8_t coef = msg[i];
        if (!coef)
            continue;
        for (int j = 1; j <= ecc; ++j)
            msg[i + j] ^= GfMul(gen[j], coef);
    }
    std::memcpy(out, msg.data() + n, ecc);
}

static int SizeOf(int ver) { return 17 + 4 * ver; }

static bool InFinder(int x, int y, int n)
{
    auto box = [](int x, int y, int ox, int oy) {
        x -= ox;
        y -= oy;
        return x >= 0 && x < 8 && y >= 0 && y < 8;
    };
    return box(x, y, 0, 0) || box(x, y, n - 8, 0) || box(x, y, 0, n - 8);
}

static void PlaceFinder(std::vector<std::vector<uint8_t>>& m, int ox, int oy)
{
    for (int y = -1; y <= 7; ++y) {
        for (int x = -1; x <= 7; ++x) {
            int xx = ox + x, yy = oy + y;
            if (xx < 0 || yy < 0 || xx >= (int)m.size() || yy >= (int)m.size())
                continue;
            bool dark = (x >= 0 && x <= 6 && y >= 0 && y <= 6) &&
                        (x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4));
            m[yy][xx] = dark ? 2 : 3; // 2 reserved dark, 3 reserved light
        }
    }
}

static void PlaceAlign(std::vector<std::vector<uint8_t>>& m, int cx, int cy)
{
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            int xx = cx + x, yy = cy + y;
            if (xx < 0 || yy < 0 || xx >= (int)m.size() || yy >= (int)m.size())
                continue;
            if (m[yy][xx] >= 2)
                continue;
            bool dark = (std::abs(x) == 2 || std::abs(y) == 2 || (x == 0 && y == 0));
            m[yy][xx] = dark ? 2 : 3;
        }
}

static bool MaskBit(int mask, int x, int y)
{
    switch (mask) {
    case 0: return ((x + y) & 1) == 0;
    case 1: return (y & 1) == 0;
    case 2: return (x % 3) == 0;
    case 3: return ((x + y) % 3) == 0;
    case 4: return (((y / 2) + (x / 3)) & 1) == 0;
    case 5: return ((x * y) % 2 + (x * y) % 3) == 0;
    case 6: return (((x * y) % 2 + (x * y) % 3) & 1) == 0;
    default: return (((x * y) % 3 + (x + y) % 2) & 1) == 0;
    }
}

static int Penalty(const std::vector<std::vector<uint8_t>>& m)
{
    const int n = (int)m.size();
    int s = 0;
    for (int y = 0; y < n; ++y) {
        int run = 1;
        for (int x = 1; x <= n; ++x) {
            bool same = x < n && (m[y][x] & 1) == (m[y][x - 1] & 1);
            if (same)
                run++;
            else {
                if (run >= 5)
                    s += 3 + (run - 5);
                run = 1;
            }
        }
    }
    for (int x = 0; x < n; ++x) {
        int run = 1;
        for (int y = 1; y <= n; ++y) {
            bool same = y < n && (m[y][x] & 1) == (m[y - 1][x] & 1);
            if (same)
                run++;
            else {
                if (run >= 5)
                    s += 3 + (run - 5);
                run = 1;
            }
        }
    }
    for (int y = 0; y < n - 1; ++y)
        for (int x = 0; x < n - 1; ++x) {
            int v = (m[y][x] & 1) + (m[y][x + 1] & 1) + (m[y + 1][x] & 1) + (m[y + 1][x + 1] & 1);
            if (v == 0 || v == 4)
                s += 3;
        }
    int dark = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (m[y][x] & 1)
                dark++;
    int perc = (dark * 100) / (n * n);
    s += (std::abs(perc - 50) / 5) * 10;
    return s;
}

std::vector<std::vector<uint8_t>> QrEncode(const std::string& text)
{
    GfInit();
    const int len = (int)text.size();
    int ver = 1;
    while (ver <= 10 && kCapM[ver] < len)
        ver++;
    if (ver > 10)
        return {};

    const int n = SizeOf(ver);
    const int dataCw = kCwM[ver];
    const int eccCw = kEccM[ver];
    const int nblk = kBlocksM[ver];

    std::vector<uint8_t> bits;
    auto put = [&](int v, int nb) {
        for (int i = nb - 1; i >= 0; --i)
            bits.push_back((uint8_t)((v >> i) & 1));
    };
    put(0x4, 4); // byte mode
    put(len, ver <= 9 ? 8 : 16);
    for (unsigned char c : text)
        put(c, 8);
    put(0, std::min(4, dataCw * 8 - (int)bits.size()));
    while (bits.size() % 8)
        bits.push_back(0);
    static const uint8_t pad[] = {0xec, 0x11};
    int pi = 0;
    while ((int)bits.size() / 8 < dataCw) {
        put(pad[pi & 1], 8);
        pi++;
    }
    bits.resize(dataCw * 8);

    std::vector<uint8_t> data(dataCw);
    for (int i = 0; i < dataCw; ++i) {
        uint8_t b = 0;
        for (int j = 0; j < 8; ++j)
            b = (uint8_t)((b << 1) | bits[i * 8 + j]);
        data[i] = b;
    }

    int shortBlk = nblk - (dataCw % nblk);
    int shortLen = dataCw / nblk;
    int longLen = shortLen + ((dataCw % nblk) ? 1 : 0);
    int eccLen = eccCw / nblk;
    std::vector<std::vector<uint8_t>> blocks(nblk);
    std::vector<std::vector<uint8_t>> eccs(nblk);
    int off = 0;
    for (int i = 0; i < nblk; ++i) {
        int dl = (i < shortBlk) ? shortLen : longLen;
        blocks[i].assign(data.begin() + off, data.begin() + off + dl);
        eccs[i].assign(eccLen, 0);
        RsEncode(blocks[i].data(), dl, eccLen, eccs[i].data());
        off += dl;
    }

    std::vector<uint8_t> final;
    int maxd = longLen;
    for (int i = 0; i < maxd; ++i)
        for (int b = 0; b < nblk; ++b)
            if (i < (int)blocks[b].size())
                final.push_back(blocks[b][i]);
    for (int i = 0; i < eccLen; ++i)
        for (int b = 0; b < nblk; ++b)
            final.push_back(eccs[b][i]);

    std::vector<std::vector<uint8_t>> base(n, std::vector<uint8_t>(n, 0));
    PlaceFinder(base, 0, 0);
    PlaceFinder(base, n - 7, 0);
    PlaceFinder(base, 0, n - 7);
    for (int i = 8; i < n - 8; ++i) {
        if (base[6][i] < 2)
            base[6][i] = (i & 1) ? 3 : 2;
        if (base[i][6] < 2)
            base[i][6] = (i & 1) ? 3 : 2;
    }
    const int* ap = kAlign[ver];
    int ac = (ver == 1) ? 0 : (ver < 7 ? 2 : 3);
    for (int i = 0; i < ac; ++i)
        for (int j = 0; j < ac; ++j) {
            int cx = ap[i], cy = ap[j];
            if ((cx <= 8 && cy <= 8) || (cx >= n - 9 && cy <= 8) || (cx <= 8 && cy >= n - 9))
                continue;
            PlaceAlign(base, cx, cy);
        }
    // version info for v>=7
    if (ver >= 7) {
        int vi = ver;
        int bitsv = vi << 12;
        int rem = bitsv;
        for (int i = 17; i >= 12; --i)
            if ((rem >> i) & 1)
                rem ^= (0x1f25 << (i - 12));
        bitsv |= rem;
        int k = 0;
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 3; ++x) {
                uint8_t d = (bitsv >> k++) & 1 ? 2 : 3;
                base[y][n - 11 + x] = d;
                base[n - 11 + x][y] = d;
            }
    }
    base[n - 8][8] = 2; // dark module

    auto fill = [&](std::vector<std::vector<uint8_t>> m, int mask) {
        int bi = 0;
        int totalBits = (int)final.size() * 8;
        for (int x = n - 1; x > 0; x -= 2) {
            if (x == 6)
                x--;
            for (int ydir = 0; ydir < n; ++ydir) {
                int y = ((x / 2) & 1) ? ydir : (n - 1 - ydir);
                for (int dx = 0; dx < 2; ++dx) {
                    int xx = x - dx;
                    if (m[y][xx] >= 2)
                        continue;
                    int bit = 0;
                    if (bi < totalBits)
                        bit = (final[bi / 8] >> (7 - (bi % 8))) & 1;
                    bi++;
                    if (MaskBit(mask, xx, y))
                        bit ^= 1;
                    m[y][xx] = (uint8_t)bit;
                }
            }
        }
        // format info
        int fmt = (1 << 3) | mask; // ECC M = 00? wait M is 00 in some tables
        // ISO: L=01, M=00, Q=11, H=10  — actually L=01, M=00, Q=11, H=10
        fmt = (0 << 3) | mask; // M = 00
        int fbits = fmt << 10;
        int r = fbits;
        for (int i = 14; i >= 10; --i)
            if ((r >> i) & 1)
                r ^= (0x537 << (i - 10));
        fbits = (fbits | r) ^ 0x5412;
        auto setF = [&](int x, int y, int b) {
            m[y][x] = b ? 1 : 0;
            if (m[y][x] < 2)
                ;
        };
        // write format — overwrite reserved format strips
        int posA[15][2] = {{8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,7},{8,8},{7,8},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8}};
        int posB[15][2];
        for (int i = 0; i < 7; ++i) {
            posB[i][0] = n - 1 - i;
            posB[i][1] = 8;
        }
        posB[7][0] = 8;
        posB[7][1] = n - 8;
        for (int i = 0; i < 7; ++i) {
            posB[8 + i][0] = 8;
            posB[8 + i][1] = n - 7 + i;
        }
        for (int i = 0; i < 15; ++i) {
            int b = (fbits >> i) & 1;
            m[posA[i][1]][posA[i][0]] = (uint8_t)b;
            m[posB[i][1]][posB[i][0]] = (uint8_t)b;
        }
        (void)setF;
        // reserved modules stay as 2/3 → convert to 1/0
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                if (m[y][x] == 2)
                    m[y][x] = 1;
                else if (m[y][x] == 3)
                    m[y][x] = 0;
            }
        return m;
    };

    int bestMask = 0, bestPen = 1 << 30;
    std::vector<std::vector<uint8_t>> best;
    for (int mask = 0; mask < 8; ++mask) {
        auto cand = fill(base, mask);
        int p = Penalty(cand);
        if (p < bestPen) {
            bestPen = p;
            bestMask = mask;
            best = std::move(cand);
        }
    }
    (void)bestMask;
    (void)InFinder;
    return best;
}
