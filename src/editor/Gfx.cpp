#include "Gfx.h"
#include "Geometry.h"
#include "Util.h"
#include <d3dcompiler.h>
#include <wincodec.h>
#include <propvarutil.h>

namespace osteon {

template <class T> static void safeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

// ---- WIC ------------------------------------------------------------------------------------

static IWICImagingFactory* wic() {
    static IWICImagingFactory* f = nullptr;
    if (!f) CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    return f;
}

bool loadImageFile(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba) {
    IWICImagingFactory* f = wic();
    if (!f) return false;
    IWICBitmapDecoder* dec = nullptr;
    if (FAILED(f->CreateDecoderFromFilename(widen(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) return false;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    bool ok = false;
    if (SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(f->CreateFormatConverter(&conv)) &&
        SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        UINT uw, uh;
        conv->GetSize(&uw, &uh);
        w = (int)uw; h = (int)uh;
        rgba.resize((size_t)w * h * 4);
        ok = SUCCEEDED(conv->CopyPixels(nullptr, w * 4, (UINT)rgba.size(), rgba.data()));
    }
    safeRelease(conv); safeRelease(frame); safeRelease(dec);
    return ok;
}

bool loadImageFileSize(const std::string& path, int& w, int& h) {
    IWICImagingFactory* f = wic();
    if (!f) return false;
    IWICBitmapDecoder* dec = nullptr;
    if (FAILED(f->CreateDecoderFromFilename(widen(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) return false;
    IWICBitmapFrameDecode* frame = nullptr;
    UINT uw = 0, uh = 0;
    bool ok = SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&uw, &uh));
    w = (int)uw; h = (int)uh;
    safeRelease(frame); safeRelease(dec);
    return ok;
}

static bool encode(const std::string& path, const GUID& container, int w, int h,
                   const std::vector<const uint8_t*>& frames, int delayCs) {
    IWICImagingFactory* f = wic();
    if (!f) return false;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    bool ok = SUCCEEDED(f->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(widen(path).c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(f->CreateEncoder(container, nullptr, &enc)) && SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache));
    bool gif = container == GUID_ContainerFormatGif;
    if (ok && gif && frames.size() > 1) {
        IWICMetadataQueryWriter* mw = nullptr;
        if (SUCCEEDED(enc->GetMetadataQueryWriter(&mw))) {
            PROPVARIANT v;
            PropVariantInit(&v);
            static UCHAR app[] = "NETSCAPE2.0";
            v.vt = VT_UI1 | VT_VECTOR; v.caub.cElems = 11; v.caub.pElems = app;
            mw->SetMetadataByName(L"/appext/application", &v);
            static UCHAR loop[] = {3, 1, 0, 0, 0}; // loop forever
            v.caub.cElems = 5; v.caub.pElems = loop;
            mw->SetMetadataByName(L"/appext/data", &v);
            mw->Release();
        }
    }
    for (const uint8_t* px : frames) {
        if (!ok) break;
        IWICBitmapFrameEncode* fe = nullptr;
        IPropertyBag2* props = nullptr;
        ok = SUCCEEDED(enc->CreateNewFrame(&fe, &props)) && SUCCEEDED(fe->Initialize(props)) && SUCCEEDED(fe->SetSize(w, h));
        IWICBitmap* bmp = nullptr;
        if (ok) ok = SUCCEEDED(f->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppRGBA, w * 4, w * h * 4, (BYTE*)px, &bmp));
        if (ok && gif) {
            IWICPalette* pal = nullptr;
            IWICFormatConverter* conv = nullptr;
            ok = SUCCEEDED(f->CreatePalette(&pal)) && SUCCEEDED(pal->InitializeFromBitmap(bmp, 256, FALSE)) &&
                 SUCCEEDED(f->CreateFormatConverter(&conv)) &&
                 SUCCEEDED(conv->Initialize(bmp, GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeErrorDiffusion, pal, 0, WICBitmapPaletteTypeCustom));
            WICPixelFormatGUID fmt = GUID_WICPixelFormat8bppIndexed;
            if (ok) ok = SUCCEEDED(fe->SetPixelFormat(&fmt)) && SUCCEEDED(fe->SetPalette(pal));
            IWICMetadataQueryWriter* mw = nullptr;
            if (ok && SUCCEEDED(fe->GetMetadataQueryWriter(&mw))) {
                PROPVARIANT v;
                PropVariantInit(&v);
                v.vt = VT_UI2; v.uiVal = (USHORT)delayCs;
                mw->SetMetadataByName(L"/grctlext/Delay", &v);
                mw->Release();
            }
            if (ok) ok = SUCCEEDED(fe->WriteSource(conv, nullptr));
            safeRelease(conv); safeRelease(pal);
        } else if (ok) {
            WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA;
            ok = SUCCEEDED(fe->SetPixelFormat(&fmt));
            if (ok && fmt != GUID_WICPixelFormat32bppRGBA) {
                IWICFormatConverter* conv = nullptr;
                ok = SUCCEEDED(f->CreateFormatConverter(&conv)) &&
                     SUCCEEDED(conv->Initialize(bmp, fmt, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
                     SUCCEEDED(fe->WriteSource(conv, nullptr));
                safeRelease(conv);
            } else if (ok) {
                ok = SUCCEEDED(fe->WritePixels(h, w * 4, w * h * 4, (BYTE*)px));
            }
        }
        if (ok) ok = SUCCEEDED(fe->Commit());
        safeRelease(bmp); safeRelease(props); safeRelease(fe);
    }
    if (ok) ok = SUCCEEDED(enc->Commit());
    safeRelease(enc); safeRelease(stream);
    return ok;
}

bool savePngFile(const std::string& path, int w, int h, const uint8_t* rgba) {
    return encode(path, GUID_ContainerFormatPng, w, h, {rgba}, 0);
}

bool saveGifFile(const std::string& path, int w, int h, const std::vector<std::vector<uint8_t>>& frames, int delayCs) {
    std::vector<const uint8_t*> ptrs;
    for (auto& f : frames) ptrs.push_back(f.data());
    return encode(path, GUID_ContainerFormatGif, w, h, ptrs, delayCs);
}

// ---- render targets -------------------------------------------------------------------------

void RenderTarget::release() {
    safeRelease(srv); safeRelease(rtv); safeRelease(tex); safeRelease(dsv); safeRelease(ds);
    w = h = 0;
}

static const char* kShader = R"(
cbuffer View : register(b0) { float4 cam; float4 size; float4 bgA; float4 bgB; };
struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 light : COLOR0; float4 dark : COLOR1; };
struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 light : COLOR0; float4 dark : COLOR1; };
Texture2D tex : register(t0);
SamplerState smp : register(s0);
PSIn vs_main(VSIn i) {
  PSIn o;
  float px = (i.pos.x - cam.x) * cam.z + size.x * 0.5;
  float py = size.y * 0.5 - (i.pos.y - cam.y) * cam.z;
  o.pos = float4(px / size.x * 2 - 1, 1 - py / size.y * 2, 0, 1);
  o.uv = i.uv; o.light = i.light; o.dark = i.dark;
  return o;
}
float4 ps_main(PSIn i) : SV_TARGET {
  float4 t = tex.Sample(smp, i.uv); // premultiplied
  float4 o;
  o.a = t.a * i.light.a;
  o.rgb = ((t.a - 1.0) * i.dark.a + 1.0 - t.rgb) * i.dark.rgb + t.rgb * i.light.rgb;
  o.rgb *= i.light.a;
  return o;
}
float4 ps_checker(PSIn i) : SV_TARGET {
  float2 c;
  if (size.w > 0.5) {
    // World-space cells: they grow and shrink with the zoom. Zoomed far out, cells double until they stay readable.
    float t = size.z;
    for (int k = 0; k < 14; k++) if (t * cam.z < 10.0) t *= 2.0;
    float2 w = float2((i.pos.x - size.x * 0.5) / cam.z + cam.x, cam.y - (i.pos.y - size.y * 0.5) / cam.z);
    c = floor(w / t);
  } else {
    c = floor(i.pos.xy / size.z);
  }
  return fmod(abs(c.x + c.y), 2.0) < 1.0 ? bgA : bgB;
}
float4 ps_stencil(PSIn i) : SV_TARGET { return float4(0, 0, 0, 0); }
)";

static ID3DBlob* compile(const char* entry, const char* target) {
    ID3DBlob* code = nullptr;
    ID3DBlob* err = nullptr;
    if (FAILED(D3DCompile(kShader, strlen(kShader), "osteon", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err))) {
        if (err) { OutputDebugStringA((const char*)err->GetBufferPointer()); err->Release(); }
        return nullptr;
    }
    safeRelease(err);
    return code;
}

bool Gfx::init(ID3D11Device* dev, ID3D11DeviceContext* c) {
    device = dev; ctx = c;
    ID3DBlob* vsb = compile("vs_main", "vs_4_0");
    ID3DBlob* psb = compile("ps_main", "ps_4_0");
    ID3DBlob* pcb = compile("ps_checker", "ps_4_0");
    ID3DBlob* psb2 = compile("ps_stencil", "ps_4_0");
    if (!vsb || !psb || !pcb || !psb2) return false;
    device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs);
    device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps);
    device->CreatePixelShader(pcb->GetBufferPointer(), pcb->GetBufferSize(), nullptr, &psChecker);
    device->CreatePixelShader(psb2->GetBufferPointer(), psb2->GetBufferSize(), nullptr, &psStencil);
    D3D11_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    device->CreateInputLayout(il, 4, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout);
    vsb->Release(); psb->Release(); pcb->Release(); psb2->Release();

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 64;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbd, nullptr, &cb);

    // Premultiplied blend modes: normal, additive, multiply, screen.
    D3D11_BLEND src[4] = {D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ONE};
    D3D11_BLEND dst[4] = {D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_SRC_COLOR};
    for (int i = 0; i < 4; i++) {
        D3D11_BLEND_DESC bd = {};
        auto& rt = bd.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = src[i]; rt.DestBlend = dst[i]; rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE; rt.DestBlendAlpha = i == 1 ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd, &blends[i]);
    }
    D3D11_BLEND_DESC nb = {};
    nb.RenderTarget[0].RenderTargetWriteMask = 0;
    device->CreateBlendState(&nb, &noColorWrite);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &raster);

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sd, &sampler);

    D3D11_DEPTH_STENCIL_DESC dd = {};
    device->CreateDepthStencilState(&dd, &dsOff);
    dd.StencilEnable = TRUE;
    dd.StencilReadMask = dd.StencilWriteMask = 0xff;
    dd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    dd.FrontFace.StencilFailOp = dd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dd.BackFace = dd.FrontFace;
    device->CreateDepthStencilState(&dd, &dsWrite);
    dd.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    dd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dd.BackFace = dd.FrontFace;
    device->CreateDepthStencilState(&dd, &dsTest);

    // 1x1 white texture for untextured fills.
    uint32_t white = 0xffffffff;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {&white, 4, 0};
    ID3D11Texture2D* t = nullptr;
    device->CreateTexture2D(&td, &init, &t);
    device->CreateShaderResourceView(t, nullptr, &whiteSrv);
    t->Release();
    return true;
}

void Gfx::shutdown() {
    images.clear();
    safeRelease(vs); safeRelease(ps); safeRelease(psChecker); safeRelease(psStencil); safeRelease(layout);
    safeRelease(vb); safeRelease(ib); safeRelease(cb);
    for (auto& b : blends) safeRelease(b);
    safeRelease(noColorWrite); safeRelease(raster); safeRelease(sampler);
    safeRelease(dsOff); safeRelease(dsWrite); safeRelease(dsTest); safeRelease(whiteSrv);
}

Image* Gfx::image(const std::string& path) {
    auto it = images.find(path);
    if (it != images.end()) return it->second.get();
    auto img = std::make_unique<Image>();
    if (!loadImageFile(path, img->w, img->h, img->rgba)) {
        images[path] = nullptr;
        return nullptr;
    }
    // Upload premultiplied, with mips for smooth zoomed-out display.
    std::vector<uint8_t> pm(img->rgba);
    for (size_t i = 0; i < pm.size(); i += 4) {
        uint32_t a = pm[i + 3];
        pm[i] = (uint8_t)(pm[i] * a / 255); pm[i + 1] = (uint8_t)(pm[i + 1] * a / 255); pm[i + 2] = (uint8_t)(pm[i + 2] * a / 255);
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = img->w; td.Height = img->h; td.MipLevels = 0; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ID3D11Texture2D* t = nullptr;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &t))) { images[path] = nullptr; return nullptr; }
    ctx->UpdateSubresource(t, 0, nullptr, pm.data(), img->w * 4, 0);
    device->CreateShaderResourceView(t, nullptr, &img->srv);
    ctx->GenerateMips(img->srv);
    t->Release();
    Image* raw = img.get();
    images[path] = std::move(img);
    return raw;
}

void Gfx::ensureTarget(RenderTarget& rt, int w, int h) {
    w = std::max(1, w); h = std::max(1, h);
    if (rt.w == w && rt.h == h && rt.tex) return;
    rt.release();
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&td, nullptr, &rt.tex);
    device->CreateRenderTargetView(rt.tex, nullptr, &rt.rtv);
    device->CreateShaderResourceView(rt.tex, nullptr, &rt.srv);
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    device->CreateTexture2D(&td, nullptr, &rt.ds);
    device->CreateDepthStencilView(rt.ds, nullptr, &rt.dsv);
    rt.w = w; rt.h = h;
}

void Gfx::begin(RenderTarget& rt, float camX, float camY, float zoom, const RenderOptions& opt) {
    current = &rt;
    float clear[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(rt.rtv, clear);
    ctx->ClearDepthStencilView(rt.dsv, D3D11_CLEAR_STENCIL | D3D11_CLEAR_DEPTH, 1, 0);
    ctx->OMSetRenderTargets(1, &rt.rtv, rt.dsv);
    D3D11_VIEWPORT vp = {0, 0, (float)rt.w, (float)rt.h, 0, 1};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster);
    ctx->IASetInputLayout(layout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &sampler);
    ctx->OMSetDepthStencilState(dsOff, 0);
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        float* f = (float*)m.pData;
        float data[16] = {camX, camY, zoom, 0, (float)rt.w, (float)rt.h, opt.checker, opt.checkerWorld ? 1.0f : 0.0f,
                          opt.bgA.r, opt.bgA.g, opt.bgA.b, opt.bgA.a, opt.bgB.r, opt.bgB.g, opt.bgB.b, opt.bgB.a};
        memcpy(f, data, sizeof data);
        ctx->Unmap(cb, 0);
    }
    ctx->VSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetConstantBuffers(0, 1, &cb);
    batchTex = nullptr;
    batchBlend = BlendMode::Normal;
    setBlend(BlendMode::Normal);
    if (opt.background) {
        // Full-viewport quad in world space.
        float hw = rt.w / 2.0f / zoom, hh = rt.h / 2.0f / zoom;
        verts = {{camX - hw, camY - hh, 0, 0, 0xffffffff, 0}, {camX - hw, camY + hh, 0, 0, 0xffffffff, 0},
                 {camX + hw, camY + hh, 0, 0, 0xffffffff, 0}, {camX + hw, camY - hh, 0, 0, 0xffffffff, 0}};
        indices = {0, 1, 2, 2, 3, 0};
        ctx->PSSetShader(psChecker, nullptr, 0);
        batchTex = whiteSrv;
        flush();
        ctx->PSSetShader(ps, nullptr, 0);
    }
}

void Gfx::setBlend(BlendMode b) {
    float f[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blends[(int)b], f, 0xffffffff);
}

void Gfx::flush() {
    if (indices.empty()) return;
    if (verts.size() > vbCap) {
        safeRelease(vb);
        vbCap = verts.size() * 2;
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(vbCap * sizeof(Vertex));
        bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &vb);
    }
    if (indices.size() > ibCap) {
        safeRelease(ib);
        ibCap = indices.size() * 2;
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(ibCap * 4);
        bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_INDEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &ib);
    }
    D3D11_MAPPED_SUBRESOURCE m;
    ctx->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    memcpy(m.pData, verts.data(), verts.size() * sizeof(Vertex));
    ctx->Unmap(vb, 0);
    ctx->Map(ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    memcpy(m.pData, indices.data(), indices.size() * 4);
    ctx->Unmap(ib, 0);
    UINT stride = sizeof(Vertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    ID3D11ShaderResourceView* srv = batchTex ? batchTex : whiteSrv;
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->DrawIndexed((UINT)indices.size(), 0, 0);
    verts.clear();
    indices.clear();
}

void Gfx::drawSkeleton(const Skeleton& sk, const std::function<std::string(const Attachment&, int)>& imagePath, const RenderOptions& opt) {
    std::vector<float> world;
    int clipEnd = -1; // slot index ending the active clip
    bool clipping = false;
    auto endClip = [&] {
        flush();
        ctx->ClearDepthStencilView(current->dsv, D3D11_CLEAR_STENCIL, 1, 0);
        ctx->OMSetDepthStencilState(dsOff, 0);
        clipping = false;
        clipEnd = -1;
    };
    for (int si : sk.drawOrder) {
        const Slot& slot = sk.slots[si];
        bool visible = sk.bones[slot.bone].active && (!opt.slotVisible || opt.slotVisible(si));
        const Attachment* att = visible ? sk.attachmentFor(slot) : nullptr;
        if (att && att->type == AttachmentType::Clipping) {
            if (clipping) endClip();
            if (sk.worldVertices(slot, *att, world) && world.size() >= 6) {
                flush();
                std::vector<int> tris = triangulate(world);
                ctx->OMSetDepthStencilState(dsWrite, 1);
                float f[4] = {0, 0, 0, 0};
                ctx->OMSetBlendState(noColorWrite, f, 0xffffffff);
                for (size_t i = 0; i + 1 < world.size(); i += 2) verts.push_back({world[i], world[i + 1], 0, 0, 0xffffffff, 0});
                for (int t : tris) indices.push_back(t);
                batchTex = whiteSrv;
                flush();
                setBlend(batchBlend);
                ctx->OMSetDepthStencilState(dsTest, 1);
                clipping = true;
                clipEnd = sk.data->findSlot(att->endSlot);
            }
            continue;
        }
        if (att && (att->type == AttachmentType::Region || att->type == AttachmentType::Mesh || att->type == AttachmentType::LinkedMesh)) {
            const Attachment* geom = att;
            if (att->type == AttachmentType::LinkedMesh) {
                int ps2 = att->parentSkin.empty() ? 0 : sk.data->findSkin(att->parentSkin);
                geom = sk.data->findAttachment(ps2, si, att->parentMesh);
            }
            Image* img = geom ? image(imagePath(*att, slot.sequenceIndex)) : nullptr;
            if (img && img->srv && sk.worldVertices(slot, *att, world)) {
                const SlotData& sd = sk.data->slots[si];
                BlendMode blend = sd.blend;
                if (img->srv != batchTex || blend != batchBlend) {
                    flush();
                    batchTex = img->srv;
                    if (blend != batchBlend) { batchBlend = blend; setBlend(blend); }
                }
                Color light = sk.color * slot.color * att->color * opt.tint;
                light.a *= opt.alphaScale;
                uint32_t lc = light.toABGR();
                uint32_t dc = slot.hasDark ? Color(slot.dark.r, slot.dark.g, slot.dark.b, 1).toABGR() : 0;
                uint32_t base = (uint32_t)verts.size();
                if (att->type == AttachmentType::Region) {
                    const float uv[8] = {0, 1, 0, 0, 1, 0, 1, 1};
                    for (int i = 0; i < 4; i++) verts.push_back({world[i * 2], world[i * 2 + 1], uv[i * 2], uv[i * 2 + 1], lc, dc});
                    for (int t : {0, 1, 2, 2, 3, 0}) indices.push_back(base + t);
                } else {
                    for (size_t i = 0; i + 1 < world.size() && i + 1 < geom->uvs.size(); i += 2)
                        verts.push_back({world[i], world[i + 1], geom->uvs[i], geom->uvs[i + 1], lc, dc});
                    for (int t : geom->triangles) indices.push_back(base + t);
                }
            }
        }
        if (clipping && si == clipEnd) endClip();
    }
    flush();
    if (clipping) endClip();
}

void Gfx::end() {
    flush();
    ID3D11RenderTargetView* none = nullptr;
    ctx->OMSetRenderTargets(1, &none, nullptr);
    current = nullptr;
}

std::vector<uint8_t> Gfx::readback(RenderTarget& rt) {
    std::vector<uint8_t> out((size_t)rt.w * rt.h * 4);
    D3D11_TEXTURE2D_DESC td = {};
    rt.tex->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
    ID3D11Texture2D* st = nullptr;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &st))) return out;
    ctx->CopyResource(st, rt.tex);
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        for (int y = 0; y < rt.h; y++) {
            const uint8_t* src = (const uint8_t*)m.pData + (size_t)y * m.RowPitch;
            uint8_t* dst = out.data() + (size_t)y * rt.w * 4;
            for (int x = 0; x < rt.w * 4; x += 4) {
                uint8_t a = src[x + 3];
                for (int c = 0; c < 3; c++) dst[x + c] = a ? (uint8_t)std::min(255, src[x + c] * 255 / a) : 0;
                dst[x + 3] = a;
            }
        }
        ctx->Unmap(st, 0);
    }
    st->Release();
    return out;
}

} // namespace osteon
