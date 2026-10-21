# -*- coding: utf-8 -*-
"""天气时钟 —— UI 字体生成工具（开放源码版）

本工具从 Windows 自带的微软雅黑字体生成项目实际使用的 UI 平滑字体
`font/UIFont_20.h`（TFT_eSPI VLW 数组，封闭字集，烧录进 app flash）。

设计要点（v11 修复、v13 增补）：
- dY 公式：dy = ASCENT - y0（TFT_eSPI VLW 规范），修复了 v10 整页文字
  下移约 22px 的错位事故。
- 封闭字集：脚本扫描仓库内所有 .ino/.h 的字符串字面量，再合并一份
  manual 增补清单（天气/番茄钟/倒数日等固定 UI 词），保证不缺字。
  若之后新增页面文案，重新跑本脚本即可把新字补进字库。

依赖：
    pip install Pillow
输入：
    C:/Windows/Fonts/msyh.ttc  （微软雅黑，Windows 自带；其他系统请改为本地中文字体路径）
输出：
    ../font/UIFont_20.h        （相对本脚本：仓库根/font/UIFont_20.h）

用法：
    python tools/genfonts.py
"""
import re, struct, glob, os
from PIL import ImageFont, Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # 仓库根目录

TTF = 'C:/Windows/Fonts/msyh.ttc'   # 中文字体源；非 Windows 请改成可用中文字体路径
SIZE = 20
FONT = ImageFont.truetype(TTF, SIZE)
ASCENT, DESCENT = FONT.getmetrics()
print(f'msyh 20px metrics: ascent={ASCENT} descent={DESCENT}')

# 常用中文标点（封闭字集必须显式包含，否则 UI 里这些符号会渲染成空白）
PUNCT = '，。、；：？！“”‘’《》（）—…·【】•※℃￥%'

# 固定 UI 词（扫描源码不一定覆盖到的硬编码标签）
manual = set(
    '一周天气今天明天周日周一二三四五六'
    '晴少云阴雾小雨冻雾中雨大雨阵雨暴雨小雪大雪雪粒雷雨冰雹'
    '多云正在获取失败双击重试长按回桌面单击下一条刷新'
    '热点西安优良轻度中度重度重实时空气质量风向最低最高温度'
    '月日℃％%:：'
    '雨夹霾轻浮尘扬沙强转霜冻暴'
    '东南西北风无持续向微级至偏~'
    '特旋定伴不有好凇间'
    # 番茄钟 / 倒数日 页面 UI 词（新增页面，必须显式纳入，避免缺字）
    '专注休息已开始暂停继续时间到倒数日暂无手机添加今天已过'
) | set(PUNCT)

# 扫描仓库源码里的字符串字面量，自动收集用到的汉字
src = ''
for f in glob.glob(os.path.join(ROOT, '*.ino')) + glob.glob(os.path.join(ROOT, '*.h')):
    if os.path.basename(f).lower().startswith('font'):
        continue
    src += open(f, encoding='utf-8', errors='ignore').read()
for s in re.findall(r'"((?:[^"\\]|\\.)*)"', src):
    manual.update(re.findall(r'[一-鿿　-〿＀-￯]', s))

ascii_ui = set(chr(c) for c in range(32, 127)) | {'℃'}
chars = sorted({c for c in manual if ord(c) >= 32} | ascii_ui)
print(f'UI 字集: {len(chars)} 个字符')


def render_alpha(ch, font):
    """渲染单字 8bit alpha，裁到 bounding box，返回 (bytes, x0, y0, w, h)"""
    bb = font.getbbox(ch)
    if bb is None:
        bb = (0, 0, 0, 0)
    x0, y0, x1, y1 = bb
    w, h = x1 - x0, y1 - y0
    if w <= 0 or h <= 0:
        return b'', x0, y0, 0, 0
    img = Image.new('L', (w, h), 0)
    ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255)
    return img.tobytes(), x0, y0, w, h


# ============ 生成 VLW ============
glyphs = []
for ch in chars:
    uni = ord(ch)
    adv = FONT.getlength(ch)
    bm, x0, y0, w, h = render_alpha(ch, FONT)
    dy = ASCENT - y0            # v11 修复：dY = 基线到点阵顶边的向上距离
    glyphs.append((uni, h, w, round(adv), dy, x0, bm))

# 头部 ascent 按实际墨迹写（loadMetrics 只增不减，避免被顶高导致整体下移）
hdr_ascent = max(g[4] for g in glyphs)
vlw = struct.pack('>6I', len(glyphs), 11, SIZE, 0, hdr_ascent, DESCENT)
for uni, h, w, adv, dy, dx, bm in glyphs:
    vlw += struct.pack('>7i', uni, h, w, adv, dy, dx, 0)
for uni, h, w, adv, dy, dx, bm in glyphs:
    vlw += bm
name = b'UIFont'
vlw += bytes([len(name)]) + name + b'\x00'
vlw += bytes([len(name)]) + name + b'\x01'

out_h = os.path.join(ROOT, 'font', 'UIFont_20.h')
with open(out_h, 'w', encoding='utf-8') as f:
    f.write('#include <pgmspace.h>\n')
    f.write('// 自动生成：msyh 20px UI 封闭字集 VLW（桌面+一周天气+番茄钟+倒数日），勿手改\n')
    f.write('// 由 tools/genfonts.py 生成；dY=ASCENT-y0 修复整页下移事故\n')
    f.write('const uint8_t UIFont_20[] PROGMEM = {\n')
    for i in range(0, len(vlw), 16):
        f.write(','.join(f'0x{b:02X}' for b in vlw[i:i+16]) + ',\n')
    f.write('};\n')
print(f'VLW: {len(glyphs)} glyphs, {len(vlw)} bytes -> {out_h}')

# ============ 自校验 ============
gCount, ver, size, mbox, asc, desc = struct.unpack('>6I', vlw[:24])
off = 24
have = {}
for i in range(gCount):
    uni, h, w, adv, dy, dx, pad = struct.unpack('>7i', vlw[off:off+28])
    have[uni] = (h, w)
    off += 28
bad_dy = [g for g in glyphs if g[4] < 0 and g[0] > 0x20]
assert not bad_dy, f'dY 为负的字形: {len(bad_dy)} 个!'
miss = [c for c in chars if ord(c) not in have]
assert not miss, f'UI 字集缺失: {"".join(miss)}'
print(f'自校验 OK: {gCount} glyphs, dY 全为正, UI 字集全覆盖')
