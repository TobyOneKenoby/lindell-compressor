#include "../Source/Compressor.h"
#include <iostream>
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
double run(double sr,double hz,lindell::Settings s,bool opposite=false){
 lindell::Compressor c;c.prepare(sr,s);double e=0;int count=0;
 for(int n=0;n<(int)(sr*2);++n){float a=(float)(.5*std::sin(2*3.141592653589793*hz*n/sr));float v[]={a,opposite?-a:a};c.tick(v,2,s);check(std::isfinite(v[0]),"finite output");check(std::abs(v[0]-(opposite?-v[1]:v[1]))<1e-7,"stereo link");if(n>sr){e+=v[0]*v[0];++count;}}
 return std::sqrt(e/count);
}
int main(){try{
 check(std::abs(lindell::Compressor::curve(-6,-18,4,0)-9)<1e-12,"4:1 static curve");
 for(double sr:{44100.,48000.,88200.,96000.,192000.}){
 lindell::Settings s;double compressed=run(sr,1000,s);check(compressed<.20&&compressed>.10,"compression amount");
 check(std::abs(compressed-run(sr,1000,s,true))<1e-8,"antiphase does not cancel detector");
 s.mix=0;double dry=run(sr,1000,s);check(std::abs(dry-.353553)<.0001,"dry unity");
 s.mix=50;double half=run(sr,1000,s);check(std::abs(half-(dry+compressed)*.5)<.0001,"linear blend");
 s.mix=100;s.bypass=1;s.output=20;check(std::abs(run(sr,1000,s)-dry)<1e-7,"bypass ignores makeup");
 s.bypass=0;s.output=0;s.ratio=1;check(std::abs(run(sr,1000,s)-dry)<1e-7,"unity ratio");
 s.ratio=4;s.hpf=30;double bass=run(sr,60,s);s.hpf=300;check(run(sr,60,s)>bass*1.3,"HPF reduces bass-triggered compression");
 s.ratio=1;check(std::abs(run(sr,60,s)-.353553)<.0001,"HPF leaves audio bass intact");
 lindell::Compressor c;c.prepare(sr);for(int i=0;i<10000;++i){float v[]={0,0};c.tick(v,2,s);check(v[0]==0,"silence");}
 }
 std::cout<<"PASS: curve, five sample rates, stereo/antiphase, dry, mix, bypass, ratio, detector HPF, silence\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
