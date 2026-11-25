import AppKit
import ImageIO

enum DopeyArt {
    private enum Tile: Int, CaseIterable { case hood, ear, face, eye, shirt, hand, foot, sideBody, tie }
    private static let images: [CGImage] = {
        let url = Bundle.main.url(forResource: "DopeyCat", withExtension: "png")!
        let source = CGImageSourceCreateWithURL(url as CFURL, nil)!
        let sheet = CGImageSourceCreateImageAtIndex(source, 0, nil)!
        return Tile.allCases.map {
            sheet.cropping(to: CGRect(x: ($0.rawValue % 4) * 360, y: ($0.rawValue / 4) * 360,
                                      width: 360, height: 360))!
        }
    }()

    private static func tile(_ ctx: CGContext, _ part: Tile, _ x: CGFloat, _ y: CGFloat,
                             _ sx: CGFloat = 1, _ sy: CGFloat = 1, _ rotation: CGFloat = 0) {
        ctx.saveGState()
        ctx.translateBy(x: x, y: y)
        ctx.rotate(by: rotation)
        ctx.scaleBy(x: sx, y: -sy)
        ctx.interpolationQuality = .high
        ctx.draw(images[part.rawValue], in: CGRect(x: -60, y: -60, width: 120, height: 120))
        ctx.restoreGState()
    }

    private static func face(_ ctx: CGContext, offended: Bool = false) {
        tile(ctx, .face, 0, 0)
        if offended {
            ctx.saveGState()
            ctx.setStrokeColor(red:0.23,green:0.19,blue:0.19,alpha:1)
            ctx.setFillColor(red:1,green:0.96,blue:0.87,alpha:1)
            ctx.setLineWidth(2.1)
            ctx.setLineCap(.round)
            for sign: CGFloat in [-1,1] {
                let x = sign*12
                let bounds = CGRect(x:x-7.5,y:-1,width:15,height:15)
                ctx.fillEllipse(in:bounds)
                ctx.strokeEllipse(in:bounds)
                ctx.move(to:CGPoint(x:x-sign*6.4,y:4.2))
                ctx.addLine(to:CGPoint(x:x+sign*6.8,y:9.1))
                ctx.move(to:CGPoint(x:sign*3,y:-6))
                ctx.addLine(to:CGPoint(x:sign*4.5,y:-9))
                ctx.strokePath()
            }
            ctx.move(to:CGPoint(x:0,y:-7))
            ctx.addLine(to:CGPoint(x:0,y:-10))
            ctx.strokePath()
            ctx.restoreGState()
            return
        }
        tile(ctx, .eye, -12, 5.4)
        tile(ctx, .eye, 12, 5.4, -1)
    }

    static func head(_ ctx: CGContext, offended: Bool = false) {
        tile(ctx, .ear, -33, -18)
        tile(ctx, .ear, 33, -18, -1)
        tile(ctx, .hood, 0, 0)
        face(ctx,offended:offended)
    }

    private static let rgb = CGColorSpaceCreateDeviceRGB()
    private static let eggBlue = CGGradient(colorSpace: rgb, colorComponents: [
        0.56, 0.64, 0.87, 1, 0.25, 0.32, 0.55, 1
    ], locations: [0, 1], count: 2)!

    static func egg(_ ctx: CGContext, _ x: CGFloat, _ y: CGFloat, _ angle: CGFloat) {
        ctx.saveGState()
        ctx.translateBy(x: x, y: y)
        ctx.rotate(by: angle)
        tile(ctx, .ear, -29, -38, 0.68, 0.68)
        tile(ctx, .ear, 29, -38, -0.68, 0.68)
        ctx.saveGState()
        let r = CatPhysics.eggRadius
        ctx.addEllipse(in: CGRect(x: -r, y: -r, width: 2*r, height: 2*r))
        ctx.clip()
        ctx.drawRadialGradient(eggBlue, startCenter: CGPoint(x: -17, y: -27), startRadius: 0,
                               endCenter: .zero, endRadius: 60, options: .drawsAfterEndLocation)
        ctx.restoreGState()
        ctx.saveGState()
        ctx.translateBy(x: 0, y: -22)
        ctx.scaleBy(x: 0.91, y: 0.91)
        face(ctx)
        ctx.restoreGState()
        tile(ctx, .hand, -42, 26, 0.90, 0.90)
        tile(ctx, .hand, 42, 26, 0.90, 0.90)
        ctx.restoreGState()
    }

    static func body(_ ctx: CGContext, _ p: CatPhysics, _ x: CGFloat, _ y: CGFloat, step: CGFloat = 0) {
        tile(ctx,.tie,x+42,y-57,1,0.84,-0.13)
        tile(ctx,.foot,x-18+9*step,y-1-3*step)
        tile(ctx,.foot,x+18-10*step,y-1+3*step)
        tile(ctx,.shirt,x,y-21,1,p.bodySquash.value,p.bodyTilt.value)
    }

    static func sleepHead(_ p: CatPhysics) -> (center: CGPoint, angle: CGFloat, scale: CGSize) {
        let nod = 0.5-0.5*cos(p.time*1.65)
        let angle = 0.39+0.10*nod+(p.headOffsetY.value-28)*0.0035
        let press = min(0.14,max(-0.03,2*(p.bodySquash.target-p.bodySquash.value)))
        let scale = CGSize(width:1+0.65*press,height:1-press)
        let cheekX = 41*scale.width, cheekY = 28*scale.height
        let center = CGPoint(x:237-cheekX*cos(angle)+cheekY*sin(angle),y:216-cheekX*sin(angle)-cheekY*cos(angle))
        return (center,angle,scale)
    }

    static func sleeping(_ ctx: CGContext, _ p: CatPhysics) {
        let breath = 0.5-0.5*cos(p.time*1.65)
        tile(ctx,.tie,169,179,0.85,0.70,3.27)
        tile(ctx,.sideBody,165,193-breath,1,1+0.028*breath)
        tile(ctx,.hand,151,162-1.6*breath,1.02,0.91,-0.16)
        tile(ctx,.hand,241,222,1.22,0.84,-0.14)
        let pose = sleepHead(p)
        ctx.saveGState()
        ctx.translateBy(x:pose.center.x,y:pose.center.y)
        ctx.rotate(by:pose.angle)
        ctx.scaleBy(x:pose.scale.width,y:pose.scale.height)
        head(ctx)
        ctx.restoreGState()
    }

    private static let sweets = [
        CGColor(red:0.89,green:0.62,blue:0.68,alpha:1),
        CGColor(red:0.97,green:0.90,blue:0.78,alpha:1),
        CGColor(red:0.69,green:0.76,blue:0.55,alpha:1)
    ]

    private static func dangoPose(_ p: CatPhysics, lower: CGFloat) -> (Int, CGFloat, CGFloat, CGFloat, CGPoint) {
        let progress = min(2.999,p.treatAge/CatPhysics.bowlEnd*3)
        let bite = Int(progress), phase = progress-CGFloat(bite)
        let lift = CatPhysics.ease(phase/0.33) * (1-CatPhysics.ease((phase-0.60)/0.35)) * (1-lower)
        let center = CGPoint(x:CatPhysics.seatAnchor.x+CGFloat(1-bite)*17*lift, y:184-39*lift+20*lower)
        return (bite,phase,lift,(-6+6*lift) * .pi/180,center)
    }

    private static func skewerContains(_ point: CGPoint, _ p: CatPhysics, lower: CGFloat) -> Bool {
        let (_,_,_,angle,center) = dangoPose(p, lower:lower)
        let dx = point.x-center.x, dy = point.y-center.y
        let along = dx*cos(angle)+dy*sin(angle)
        return hypot(max(0,abs(along)-47),dy*cos(angle)-dx*sin(angle)) <= 15
    }

    static func dangoAt(_ point: CGPoint, _ p: CatPhysics) -> Bool {
        skewerContains(point, p, lower:p.dangoLowering) || (p.guardingDango && skewerContains(point, p, lower:0))
    }

    static func eating(_ ctx: CGContext, _ p: CatPhysics) {
        let (bite,phase,lift,angle,center) = dangoPose(p, lower:p.dangoLowering)
        let chew = CatPhysics.ease((phase-0.5)/0.12) * (1-CatPhysics.ease((phase-0.85)/0.15))
        let x = CatPhysics.seatAnchor.x
        body(ctx,p,x,CatPhysics.seatAnchor.y)
        ctx.saveGState()
        ctx.translateBy(x:x,y:125+3*lift+2*chew*sin(phase*31))
        ctx.rotate(by:(3*lift-2*chew) * .pi/180)
        head(ctx,offended:p.guardingDango)
        ctx.restoreGState()
        ctx.saveGState()
        ctx.translateBy(x:center.x,y:center.y)
        ctx.rotate(by:angle)
        ctx.setStrokeColor(red:0.62,green:0.45,blue:0.28,alpha:1)
        ctx.setLineWidth(2.8)
        ctx.move(to:CGPoint(x:-47,y:0))
        ctx.addLine(to:CGPoint(x:47,y:0))
        ctx.strokePath()
        for i in bite..<3 {
            let r: CGFloat = 8.6 * (i == bite ? 1-CatPhysics.ease((phase-0.43)/0.13) : 1)
            let cx = CGFloat(i-1)*17
            ctx.setFillColor(sweets[i])
            ctx.fillEllipse(in:CGRect(x:cx-r,y:-r,width:2*r,height:2*r))
            ctx.setFillColor(red:1,green:0.98,blue:0.91,alpha:0.45)
            ctx.fillEllipse(in:CGRect(x:cx-r*0.53,y:-r*0.56,width:r*0.52,height:r*0.30))
        }
        tile(ctx,.hand,-40,3,0.87,0.87)
        tile(ctx,.hand,40,3,0.87,0.87)
        ctx.restoreGState()
    }

    private static let stain = CGGradient(colorSpace: rgb, colorComponents: [
        0.392,0.325,0.294,1, 0.239,0.200,0.192,1
    ], locations:[0,1],count:2)!

    private static func barrel(_ ctx: CGContext) {
        let wood = CGMutablePath()
        wood.move(to:CGPoint(x:-36,y:-38))
        wood.addCurve(to:CGPoint(x:-36,y:38),control1:CGPoint(x:-46,y:-15),control2:CGPoint(x:-46,y:17))
        wood.addCurve(to:CGPoint(x:36,y:38),control1:CGPoint(x:-16,y:44),control2:CGPoint(x:16,y:44))
        wood.addCurve(to:CGPoint(x:36,y:-38),control1:CGPoint(x:46,y:17),control2:CGPoint(x:46,y:-15))
        wood.closeSubpath()
        ctx.saveGState()
        ctx.addPath(wood)
        ctx.clip()
        ctx.drawLinearGradient(stain,start:CGPoint(x:-43,y:0),end:CGPoint(x:43,y:0),options:[.drawsBeforeStartLocation,.drawsAfterEndLocation])
        ctx.setStrokeColor(red:0.36,green:0.18,blue:0.16,alpha:0.71)
        ctx.setLineWidth(1.7)
        for x: CGFloat in [-26,-10,8,24] {
            ctx.move(to:CGPoint(x:x,y:-39))
            ctx.addCurve(to:CGPoint(x:x,y:42),control1:CGPoint(x:x*1.25,y:-13),control2:CGPoint(x:x*1.25,y:15))
        }
        ctx.strokePath()
        for y: CGFloat in [-35,-5,32] {
            ctx.setFillColor(red:0.31,green:0.33,blue:0.41,alpha:1)
            ctx.fill(CGRect(x:-46,y:y,width:92,height:y == -5 ? 13 : 5))
            ctx.setFillColor(red:0.60,green:0.63,blue:0.69,alpha:1)
            ctx.fill(CGRect(x:-46,y:y,width:92,height:2))
            if y == -5 {
                for x: CGFloat in [-26,0,26] { ctx.fillEllipse(in:CGRect(x:x,y:y+5,width:3,height:3)) }
            }
        }
        ctx.restoreGState()
        ctx.setStrokeColor(red:0.31,green:0.21,blue:0.23,alpha:1)
        ctx.setLineWidth(2)
        ctx.addPath(wood)
        ctx.strokePath()
        let lid = CGRect(x:-36,y:-43,width:72,height:10)
        ctx.setFillColor(red:0.333,green:0.306,blue:0.302,alpha:1)
        ctx.fillEllipse(in:lid)
        ctx.strokeEllipse(in:lid)
    }

    static func barrelReaction(_ ctx: CGContext, _ p: CatPhysics) {
        let overhead: CGFloat = 0.48, release: CGFloat = 1.32, impact: CGFloat = 1.84
        let t = p.treatAge - CatPhysics.bowlEnd
        let raise = CatPhysics.ease(t/overhead)
        let flight = min(1,max(0,(t-release)/(impact-release)))
        let settle = CatPhysics.ease((t-impact)/0.66)
        let stance = raise*(1-settle)
        let x = CatPhysics.seatAnchor.x
        let zoom = 1-0.43*stance
        ctx.saveGState()
        ctx.translateBy(x:x,y:244)
        ctx.scaleBy(x:zoom,y:zoom)
        ctx.translateBy(x:-x,y:-244)
        body(ctx,p,x,CatPhysics.seatAnchor.y,step:stance)
        ctx.saveGState()
        ctx.translateBy(x:x,y:127+4*sin(raise * .pi)+5*flight*(1-settle))
        ctx.rotate(by:4*flight*(1-settle) * .pi/180)
        head(ctx)
        ctx.restoreGState()
        let out = CatPhysics.ease(flight)
        let handSpread = 23+9*stance+33*sin(raise * .pi)+18*out*(1-settle)
        let handY = 194-110*stance+60*out*(1-settle)
        let palm = 1+0.29*stance
        for sign: CGFloat in [-1,1] { tile(ctx,.hand,x+sign*handSpread,handY,palm,palm) }
        ctx.restoreGState()

        guard t >= overhead else { return }
        if t < impact {
            ctx.saveGState()
            let depth = flight*flight
            ctx.translateBy(x:x+7*sin(flight * .pi),y:83+67*depth)
            ctx.rotate(by:53*flight * .pi/180)
            ctx.scaleBy(x:1.27+2.05*depth,y:1.48+2.12*depth)
            barrel(ctx)
            ctx.restoreGState()
            return
        }

        let age = min(1,max(0,(t-impact)/0.98))
        let expansion = 1-(1-age)*(1-age)*(1-age)
        let puffs: [(CGFloat, CGFloat, CGFloat)] = [
            (-83,-29,27),(-28,-62,32),(51,-53,29),(89,2,25),(53,42,33),
            (-41,41,32),(-96,17,19),(-52,-15,41),(14,-26,43),(44,9,38),(-14,17,40)
        ]
        let fade = 1-CatPhysics.ease((age-0.16)/0.84)
        for (dx,dy,radius) in puffs {
            let cx = x+dx*(0.25+expansion)+8*age*sin(dx)
            let cy = 150+dy*(0.22+expansion)-31*age*age
            let r = radius*(0.53+0.77*expansion)
            let gray = (134+24*sin(dx*0.13))/255
            let bright = gray+30/255
            let smoke = CGGradient(colorSpace:rgb, colorComponents:[
                bright,bright,bright,243/255*fade, gray,gray,gray,205/255*fade, gray,gray,gray,0
            ], locations:[0,0.74,1], count:3)!
            ctx.saveGState()
            ctx.translateBy(x:cx,y:cy)
            ctx.scaleBy(x:r,y:0.86*r)
            ctx.drawRadialGradient(smoke,startCenter:CGPoint(x:-0.15,y:-0.31),startRadius:0,
                                   endCenter:.zero,endRadius:1,options:[])
            ctx.restoreGState()
        }
    }

    static func hands(_ ctx: CGContext, _ p: CatPhysics, _ stylus: Bool) {
        let reach = p.punchReach.value
        for left in [reach >= 0, reach < 0] {
            var pos = left ? p.leftPaw.point : (stylus ? p.penGrip : p.rightPaw.point)
            if stylus && !left { pos.x -= 11 }
            else { pos.y -= 7 * CatPhysics.ease((pos.y-204)/32) }
            let thrust = max(0, min(1, left ? -reach : reach))
            let scale = 1 + 1.85 * thrust
            tile(ctx, .hand, pos.x, pos.y, scale, scale)
            if thrust > 0.01 {
                let r = 15.2 * scale
                let shadeR = 0.74 * r
                ctx.saveGState()
                ctx.setFillColor(red: 174/255, green: 145/255, blue: 111/255, alpha: 218/255 * CatPhysics.ease(thrust))
                ctx.fillEllipse(in: CGRect(x: pos.x - shadeR, y: pos.y + 0.09*r - shadeR,
                                           width: 2*shadeR, height: 2*shadeR))
                ctx.restoreGState()
            }
        }
    }
}
