#pragma once
#include <memory>
#include <stdexcept>
#include <objidl.h>
#include <gdiplus.h>
#include "CatPhysics.hpp"

namespace DopeyArt {
enum Tile { Hood, Ear, Face, Eye, Shirt, Hand, Foot, SideBody, Tie };

// Keep decoded pixels independent of the resource stream and release before GDI+ shuts down.
inline std::unique_ptr<Gdiplus::Bitmap> atlas;
inline void release() { atlas.reset(); }

inline Gdiplus::Bitmap& artwork() {
    if (!atlas) {
        auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
        if (!resource) throw std::runtime_error("Missing DopeyCat artwork");
        const DWORD size = SizeofResource(nullptr, resource);
        const void* data = LockResource(LoadResource(nullptr, resource));
        IStream* stream;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) throw std::bad_alloc();
        LARGE_INTEGER start{};
        const HRESULT written = stream->Write(data, size, nullptr);
        const HRESULT rewound = stream->Seek(start, STREAM_SEEK_SET, nullptr);
        if (SUCCEEDED(written) && SUCCEEDED(rewound)) {
            Gdiplus::Bitmap image(stream);
            if (image.GetLastStatus() == Gdiplus::Ok)
                atlas.reset(image.Clone(0, 0, (INT)image.GetWidth(), (INT)image.GetHeight(), PixelFormat32bppPARGB));
        }
        stream->Release();
        if (!atlas || atlas->GetLastStatus() != Gdiplus::Ok) throw std::runtime_error("Invalid DopeyCat artwork");
    }
    return *atlas;
}

inline void tile(Gdiplus::Graphics& g, Tile part, float x, float y, float sx = 1, float sy = 1, float rotation = 0) {
    const auto st = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(rotation * 57.2957795f);
    g.ScaleTransform(sx, sy);
    g.DrawImage(&artwork(), Gdiplus::RectF(-60,-60,120,120),
                float(part % 4 * 360), float(part / 4 * 360), 360, 360, Gdiplus::UnitPixel);
    g.Restore(st);
}

inline void face(Gdiplus::Graphics& g, bool offended = false) {
    tile(g, Face, 0, 0);
    if (offended) {
        using namespace Gdiplus;
        Pen ink(Color(255,59,48,48),2.1f);
        ink.SetStartCap(LineCapRound); ink.SetEndCap(LineCapRound);
        SolidBrush white(Color(255,255,245,222));
        for (float sign : {-1.f,1.f}) {
            const float x = sign*12;
            g.FillEllipse(&white,x-7.5f,-1.f,15.f,15.f);
            g.DrawEllipse(&ink,x-7.5f,-1.f,15.f,15.f);
            g.DrawLine(&ink,x-sign*6.4f,4.2f,x+sign*6.8f,9.1f);
            g.DrawLine(&ink,sign*3,-6.f,sign*4.5f,-9.f);
        }
        g.DrawLine(&ink,0.f,-7.f,0.f,-10.f);
        return;
    }
    tile(g, Eye, -12, 5.4f);
    tile(g, Eye, 12, 5.4f, -1);
}

inline void head(Gdiplus::Graphics& g, bool offended = false) {
    tile(g, Ear, -33, -18);
    tile(g, Ear, 33, -18, -1);
    tile(g, Hood, 0, 0);
    face(g,offended);
}

// Soft blue afterimages where the egg just rolled, oldest (faintest, smallest) first.
inline void eggTrail(Gdiplus::Graphics& g, float x, float y, const CatPhysics& p) {
    using namespace Gdiplus;
    constexpr float r = CatPhysics::EggRadius;
    for (const auto& s : p.eggTrail) {
        const float ox = s.travel.x - p.eggTravel.x, oy = s.travel.y - p.eggTravel.y;
        const float d = std::hypot(ox, oy);
        if (d < 4.0f) continue;
        const float t = std::clamp(s.age / CatPhysics::EggTrailLife, 0.0f, 1.0f);
        const float fade = (1 - t) * (1 - t);
        // Nearer samples hide behind the egg, so they can be stronger without forming a solid slab.
        const float alpha = 0.20f * fade * std::min(1.0f, d / (1.2f * r));
        const float rad = r * (0.92f - 0.42f * t);
        SolidBrush ghost(Color((BYTE)std::lround(255 * alpha), 120, 142, 206));
        g.FillEllipse(&ghost, x + ox - rad, y + oy - rad, 2 * rad, 2 * rad);
    }
}

inline void egg(Gdiplus::Graphics& g, float x, float y, float angle) {
    using namespace Gdiplus;
    auto state = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(angle * 57.2957795f);
    tile(g, Ear, -29, -38, 0.68f, 0.68f);
    tile(g, Ear, 29, -38, -0.68f, 0.68f);
    GraphicsPath shell;
    constexpr float r = CatPhysics::EggRadius;
    shell.AddEllipse(-r, -r, 2*r, 2*r);
    PathGradientBrush blue(&shell);
    blue.SetCenterPoint(PointF(-17,-27));
    blue.SetCenterColor(Color(255,143,164,222));
    Color edge(255,65,81,140);
    INT count = 1;
    blue.SetSurroundColors(&edge, &count);
    g.FillPath(&blue, &shell);
    auto inset = g.Save();
    g.TranslateTransform(0,-22);
    g.ScaleTransform(0.91f,0.91f);
    face(g);
    g.Restore(inset);
    tile(g, Hand, -42,26, 0.90f,0.90f);
    tile(g, Hand, 42,26, 0.90f,0.90f);
    g.Restore(state);
}

inline void body(Gdiplus::Graphics& g, const CatPhysics& p, float x, float y, float step = 0) {
    tile(g, Tie, x+42, y-57, 1, .84f, -.13f);
    tile(g, Foot, x-18+9*step, y-1-3*step);
    tile(g, Foot, x+18-10*step, y-1+3*step);
    tile(g, Shirt, x, y-21, 1, p.bodySquash.value, p.bodyTilt.value);
}

struct SleepingHead { Pt center; float angle; Pt scale; };
inline SleepingHead sleepHead(const CatPhysics& p) {
    const float nod = .5f-.5f*std::cos(p.time*1.65f);
    const float angle = (.39f+.10f*nod)+(p.headOffsetY.value-28)*.0035f;
    // Rotate about the cheek resting on the paw, so the neck stays tucked into the smock.
    const float press = std::clamp(2*(p.bodySquash.target-p.bodySquash.value), -.03f, .14f);
    const Pt scale{1+.65f*press, 1-press};
    const float cheekX = 41*scale.x, cheekY = 28*scale.y;
    return {{237-cheekX*std::cos(angle)+cheekY*std::sin(angle),
             216-cheekX*std::sin(angle)-cheekY*std::cos(angle)}, angle, scale};
}

inline void sleeping(Gdiplus::Graphics& g, const CatPhysics& p) {
    const float breath = .5f-.5f*std::cos(p.time*1.65f);
    tile(g,Tie,169,179,.85f,.70f,3.27f);
    tile(g,SideBody,165,193-breath,1,1+.028f*breath);
    tile(g,Hand,151,162-1.6f*breath,1.02f,.91f,-.16f);
    tile(g,Hand,241,222,1.22f,.84f,-.14f);
    const auto pose = sleepHead(p);
    const auto neck = g.Save();
    g.TranslateTransform(pose.center.x,pose.center.y);
    g.RotateTransform(pose.angle*57.2957795f);
    g.ScaleTransform(pose.scale.x,pose.scale.y);
    head(g);
    g.Restore(neck);
}

struct DangoPose { int bite; float phase,lift,angle; Pt center; };
inline DangoPose dangoPose(const CatPhysics& p, float lower) {
    const float progress = std::min(2.999f, p.treatAge()/CatPhysics::BowlEnd*3);
    const int bite = static_cast<int>(progress);
    const float phase = progress-bite;
    const float lift = CatPhysics::ease(phase/0.33f) * (1-CatPhysics::ease((phase-0.60f)/0.35f)) * (1-lower);
    return {bite,phase,lift,-6+6*lift,{CatPhysics::SeatAnchor.x+(1-bite)*17*lift,184-39*lift+20*lower}};
}

inline bool skewerContains(Pt point, const DangoPose& pose) {
    const float angle = pose.angle*0.017453293f, dx = point.x-pose.center.x, dy = point.y-pose.center.y;
    const float along = dx*std::cos(angle)+dy*std::sin(angle);
    const float normal = dy*std::cos(angle)-dx*std::sin(angle);
    return std::hypot(std::max(0.f,std::abs(along)-47),normal) <= 15;
}

inline bool dangoAt(Pt point, const CatPhysics& p) {
    // Keep the first click's position reachable while the skewer moves under the pointer.
    return skewerContains(point, dangoPose(p, p.dangoLowering())) ||
           (p.guardingDango() && skewerContains(point, dangoPose(p, 0)));
}

inline void eating(Gdiplus::Graphics& g, const CatPhysics& p) {
    using namespace Gdiplus;
    auto [bite,phase,lift,angle,center] = dangoPose(p, p.dangoLowering());
    const float chew = CatPhysics::ease((phase-0.5f)/0.12f)*(1-CatPhysics::ease((phase-0.85f)/0.15f));
    const float x = CatPhysics::SeatAnchor.x;
    body(g,p,x,CatPhysics::SeatAnchor.y);
    auto facePose = g.Save();
    g.TranslateTransform(x,125+3*lift+2*chew*std::sin(phase*31));
    g.RotateTransform(3*lift-2*chew);
    head(g,p.guardingDango());
    g.Restore(facePose);
    auto skewer = g.Save();
    g.TranslateTransform(center.x,center.y);
    g.RotateTransform(angle);
    Pen bamboo(Color(255,157,114,72),2.8f);
    g.DrawLine(&bamboo,-47,0,47,0);
    const Color colors[]{Color(255,226,159,173), Color(255,248,230,200), Color(255,175,195,141)};
    for(int i=bite;i<3;++i) {
        const float r = 8.6f*(i==bite ? 1-CatPhysics::ease((phase-0.43f)/0.13f) : 1);
        if (r <= 0) continue;
        const float cx = (i-1)*17.0f;
        SolidBrush sweet(colors[i]);
        g.FillEllipse(&sweet,cx-r,-r,2*r,2*r);
        SolidBrush sheen(Color(115,255,250,232));
        g.FillEllipse(&sheen,cx-r*.53f,-r*.56f,r*.52f,r*.30f);
    }
    tile(g,Hand,-40,3,0.87f,0.87f);
    tile(g,Hand,40,3,0.87f,0.87f);
    g.Restore(skewer);
}

inline void barrel(Gdiplus::Graphics& g) {
    using namespace Gdiplus;
    GraphicsPath wood;
    wood.AddBezier(-36,-38,-46,-15,-46,17,-36,38);
    wood.AddBezier(-36,38,-16,44,16,44,36,38);
    wood.AddBezier(36,38,46,17,46,-15,36,-38);
    wood.CloseFigure();
    const auto boards = g.Save();
    g.SetClip(&wood,CombineModeIntersect);
    LinearGradientBrush stain(PointF(-43,0),PointF(43,0),Color(255,100,83,75),Color(255,61,51,49));
    g.FillPath(&stain,&wood);
    Pen seam(Color(180,92,45,40),1.7f);
    for (int x : {-26,-10,8,24}) g.DrawBezier(&seam,float(x),-39.f,x*1.25f,-13.f,x*1.25f,15.f,float(x),42.f);
    SolidBrush band(Color(255,80,84,104)), rim(Color(255,153,161,176));
    for (int y : {-35,-5,32}) {
        g.FillRectangle(&band,-46,y,92,y == -5 ? 13 : 5);
        g.FillRectangle(&rim,-46,y,92,2);
        for (int x : {-26,0,26}) if (y == -5) g.FillEllipse(&rim,x,y+5,3,3);
    }
    g.Restore(boards);
    Pen outline(Color(255,78,54,58),2.f);
    g.DrawPath(&outline,&wood);
    SolidBrush lid(Color(255,85,78,77));
    g.FillEllipse(&lid,-36,-43,72,10);
    g.DrawEllipse(&outline,-36,-43,72,10);
}

inline void barrelReaction(Gdiplus::Graphics& g, const CatPhysics& p) {
    using namespace Gdiplus;
    constexpr float overhead = .48f, release = 1.32f, impact = 1.84f;
    const float t = p.treatAge() - CatPhysics::BowlEnd;
    const float raise = CatPhysics::ease(t / overhead);
    const float flight = std::clamp((t-release)/(impact-release), 0.f, 1.f);
    const float settle = CatPhysics::ease((t-impact)/.66f);
    const float stance = raise*(1-settle);
    const float x = CatPhysics::SeatAnchor.x;
    // Frame the tall, loaded pose around the planted foot.
    const auto stage = g.Save();
    const float zoom = 1-.43f*stance;
    g.TranslateTransform(x,244);
    g.ScaleTransform(zoom,zoom);
    g.TranslateTransform(-x,-244);
    body(g,p,x,CatPhysics::SeatAnchor.y,stance);
    const auto neck = g.Save();
    g.TranslateTransform(x,127+4*std::sin(raise*3.14159f)+5*flight*(1-settle));
    g.RotateTransform(4*flight*(1-settle));
    head(g);
    g.Restore(neck);
    const float out = CatPhysics::ease(flight);
    const float handSpread = 23+9*stance+33*std::sin(raise*3.14159f)+18*out*(1-settle);
    const float handY = 194-110*stance+60*out*(1-settle);
    const float palm = 1+.29f*stance;
    for(float sign : {-1.f,1.f}) tile(g,Hand,x+sign*handSpread,handY,palm,palm);
    g.Restore(stage);

    if (t < overhead) return;
    if (t < impact) {
        const auto projectile = g.Save();
        const float depth = flight*flight;
        g.TranslateTransform(x+7*std::sin(flight*3.14159f),83+67*depth);
        g.RotateTransform(53*flight);
        g.ScaleTransform(1.27f+2.05f*depth,1.48f+2.12f*depth);
        barrel(g);
        g.Restore(projectile);
        return;
    }

    const float age = std::clamp((t-impact)/.98f,0.f,1.f);
    const float expansion = 1-(1-age)*(1-age)*(1-age);
    // Offset lobes overlap at impact and drift apart as the cloud rises.
    struct Puff { float dx, dy, radius; };
    constexpr Puff puffs[] = {
        {-83,-29,27},{-28,-62,32},{51,-53,29},{89,2,25},{53,42,33},
        {-41,41,32},{-96,17,19},{-52,-15,41},{14,-26,43},{44,9,38},{-14,17,40}
    };
    const float fade = 1-CatPhysics::ease((age-.16f)/.84f);
    for(const auto& puff : puffs) {
        const float cx = x+puff.dx*(.25f+expansion)+8*age*std::sin(puff.dx);
        const float cy = 150+puff.dy*(.22f+expansion)-31*age*age;
        const float r = puff.radius*(.53f+.77f*expansion);
        const BYTE gray = BYTE(134+24*std::sin(puff.dx*.13f));
        GraphicsPath billow;
        billow.AddEllipse(cx-r,cy-r*.86f,2*r,1.72f*r);
        PathGradientBrush smoke(&billow);
        smoke.SetCenterPoint(PointF(cx-r*.15f,cy-r*.27f));
        const Color tones[] = {Color(0,gray,gray,gray), Color(BYTE(205*fade),gray,gray,gray),
                               Color(BYTE(243*fade),gray+30,gray+30,gray+30)};
        REAL stops[] = {0,.26f,1}; // MinGW's GDI+ header takes a non-const pointer
        smoke.SetInterpolationColors(tones,stops,3);
        g.FillPath(&smoke,&billow);
    }
}

inline void hands(Gdiplus::Graphics& g, const CatPhysics& p, bool stylus) {
    const float reach = p.punchReach.value;
    for (bool left : {reach >= 0, reach < 0}) {
        Pt pos = left ? p.leftPaw.point() : (stylus ? p.penGrip() : p.rightPaw.point());
        // Position the lower edge on the keycap, keeping its legend readable.
        if (stylus && !left) pos.x -= 11.0f;
        else pos.y -= 7.0f * CatPhysics::ease((pos.y-204.0f)/32.0f);
        const float extension = std::clamp(left ? -reach : reach, 0.0f, 1.0f);
        const float scale = 1 + 1.85f * extension;
        tile(g, Hand, pos.x, pos.y, scale, scale);
        if (extension > 0.01f) {
            const float r = 15.2f * scale;
            const float shadeR = 0.74f * r;
            Gdiplus::SolidBrush shade(Gdiplus::Color(BYTE(218 * CatPhysics::ease(extension)), 174, 145, 111));
            g.FillEllipse(&shade, pos.x - shadeR, pos.y + 0.09f*r - shadeR, 2*shadeR, 2*shadeR);
        }
    }
}
} // namespace DopeyArt
