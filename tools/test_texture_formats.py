"""Compile actual pure texture routines with ASan/UBSan, without GPU stubs.

GXM format enum labels in the packed-float fixture are synthetic; this checks
conversion math/layout, not texture descriptor decoding or Apple sampling.
"""
from pathlib import Path
import os,subprocess,tempfile

root=Path(__file__).resolve().parents[1]
text=(root/'vita3k/renderer/src/texture/format.cpp').read_text()
bc=text[text.index('static void decompress_block_bc_color('):text.index('/**\n * \\brief Solves Z-order')]
packed=text[text.index('static uint16_t f10_to_f16('):text.index('// Based on this:')]
code=r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
using SceGxmTextureFormat=int;
constexpr int SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ABGR=1;
constexpr int SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ARGB=2;
constexpr int SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1BGR=3;
constexpr int SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1RGB=4;
PRODUCTION_ROUTINES
double half_value(uint16_t h) {
 unsigned e=(h>>10)&31,m=h&1023;
 if(e==31)return m?NAN:INFINITY;
 return e?std::ldexp(1.0+m/1024.0,int(e)-15):std::ldexp(double(m),-24);
}
int main() {
 for(uint16_t f10=0;f10<1024;f10++) {
  unsigned e=f10>>5,m=f10&31;
  double expected=e==31?(m?NAN:INFINITY):e?std::ldexp(1.0+m/32.0,int(e)-15):std::ldexp(double(m),-19);
  double actual=half_value(f10_to_f16(f10));
  assert(std::isnan(expected)?std::isnan(actual):expected==actual);
 }
 const uint16_t alphas[]={0,0x3555,0x3955,0x3c00};
 for(int format=1;format<=5;format++)for(uint32_t alpha=0;alpha<4;alpha++) {
  uint32_t rgb=0x1e0u|(0x200u<<10)|(1u<<20);
  uint32_t input=format<=4?rgb|(alpha<<30):(rgb<<2)|alpha;
  std::array<uint16_t,4> result{};
  convert_u2f10f10f10_to_f16f16f16f16(result.data(),&input,1,1,format);
  std::array<uint16_t,4> expected=format<=4?std::array<uint16_t,4>{0x3c00,0x4000,0x20,alphas[alpha]}:std::array<uint16_t,4>{alphas[alpha],0x3c00,0x4000,0x20};
  assert(result==expected);
 }
 std::array<uint32_t,16> pixels{};
 const uint8_t bc1[]={0,0,255,255,255,255,255,255};
 decompress_block_bc1(bc1,pixels.data());
 for(auto p:pixels)assert(p==0);
 uint8_t bc2[16];std::fill_n(bc2,8,255);std::copy_n(bc1,8,bc2+8);
 decompress_block_bc2(bc2,pixels.data());
 for(auto p:pixels)assert(p==0xffaaaaaa); // BC2 always uses the four-color palette.
 uint8_t bc3[16]{};bc3[0]=255;std::copy_n(bc1,8,bc3+8);
 decompress_block_bc3(bc3,pixels.data());
 for(auto p:pixels)assert(p==0xffaaaaaa);
 int extents=0;
 for(uint32_t width:{1u,2u,3u,4u,5u,7u,8u,9u,17u})
 for(uint32_t height:{1u,2u,3u,4u,5u,7u,8u,13u})
 for(uint8_t format=1;format<=7;format++) {
  const unsigned block=(format==1||format==4||format==5)?8:16;
  const unsigned bytes=format<=3?4:format<=5?1:2;
  std::vector<uint8_t> input(((width+3)/4)*((height+3)/4)*block);
  const size_t output_bytes=size_t(width)*height*bytes;
  std::vector<uint32_t> storage((output_bytes+3)/4+8,0xa5a5a5a5);
  decompress_bc_image(width,height,input.data(),storage.data(),format);
  auto output=reinterpret_cast<const uint8_t*>(storage.data());
  for(size_t i=output_bytes;i<storage.size()*4;i++)assert(output[i]==0xa5);
  extents++;
 }
 std::cout<<"PASS: 1024 F10 values, 20 alpha layouts, BC1/2/3 palettes, "<<extents<<" exact output extents\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory)
    cpp=tmp/'formats.cpp';cpp.write_text(code.replace('PRODUCTION_ROUTINES',bc+'\n'+packed))
    binary=tmp/'formats'
    subprocess.run(['c++','-std=c++20','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
