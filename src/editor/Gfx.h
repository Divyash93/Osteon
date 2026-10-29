// Direct3D 11 rendering of skeletons into offscreen targets, plus WIC image I/O.
#pragma once
#include "Skeleton.h"
#include <d3d11.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace osteon {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba; // straight alpha, kept for hull tracing and packing
    ID3D11ShaderResourceView* srv = nullptr;
    ~Image() { if (srv) srv->Release(); }
};

// WIC helpers (paths are UTF-8).
bool loadImageFile(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba);
bool loadImageFileSize(const std::string& path, int& w, int& h);
bool savePngFile(const std::string& path, int w, int h, const uint8_t* rgba);
/** Animated GIF; frames are RGBA (straight alpha), delay in 1/100 s. */
bool saveGifFile(const std::string& path, int w, int h, const std::vector<std::vector<uint8_t>>& frames, int delayCs);

struct RenderTarget {
    int w = 0, h = 0;
    ID3D11Texture2D* tex = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11Texture2D* ds = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    void release();
    ~RenderTarget() { release(); }
};

struct RenderOptions {
    bool background = true;      // checkerboard; false = transparent (exports)
    Color bgA{0.22f, 0.22f, 0.22f, 1}, bgB{0.26f, 0.26f, 0.26f, 1};
    float checker = 16;          // checker cell in pixels
    std::function<bool(int slot)> slotVisible; // editor visibility toggles
    float alphaScale = 1;        // ghosting
    Color tint{1, 1, 1, 1};
};

class Gfx {
public:
    bool init(ID3D11Device* device, ID3D11DeviceContext* ctx);
    void shutdown();

    /** Returns the cached image for an absolute file path (loads on first use; nullptr if missing). */
    Image* image(const std::string& path);
    void reloadImages() { images.clear(); }

    void ensureTarget(RenderTarget& rt, int w, int h);
    /** Starts rendering into rt. `view` maps world (y-up) to pixel space: px = (x - camX) * zoom + w/2. */
    void begin(RenderTarget& rt, float camX, float camY, float zoom, const RenderOptions& opt);
    /** `imagePath(attachment, sequenceIndex)` resolves an attachment's image file (-1 = setup frame). */
    void drawSkeleton(const Skeleton& sk, const std::function<std::string(const Attachment&, int)>& imagePath, const RenderOptions& opt);
    void end();
    /** Reads the target back as straight-alpha RGBA. */
    std::vector<uint8_t> readback(RenderTarget& rt);

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* ctx = nullptr;

private:
    struct Vertex { float x, y, u, v; uint32_t light, dark; };
    void flush();
    void setBlend(BlendMode b);
    void drawBackground(const RenderOptions& opt);

    std::map<std::string, std::unique_ptr<Image>> images;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11PixelShader* psChecker = nullptr;
    ID3D11PixelShader* psStencil = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    ID3D11Buffer* cb = nullptr;
    ID3D11BlendState* blends[4] = {};
    ID3D11BlendState* noColorWrite = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    ID3D11SamplerState* sampler = nullptr;
    ID3D11DepthStencilState* dsOff = nullptr, *dsWrite = nullptr, *dsTest = nullptr;
    ID3D11ShaderResourceView* whiteSrv = nullptr;
    RenderTarget* current = nullptr;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    ID3D11ShaderResourceView* batchTex = nullptr;
    BlendMode batchBlend = BlendMode::Normal;
    size_t vbCap = 0, ibCap = 0;
};

} // namespace osteon
