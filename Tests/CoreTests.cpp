#include "../Source/Compressor.h"
#include <iostream>
#include "LegacyCompressor.h"
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
double run(double sr,double hz,lindell::Settings s,bool opposite=false){
 lindell::Compressor c;c.prepare(sr,s);double e=0;int count=0;
 for(int n=0;n<(int)(sr*2);++n){float a=(float)(.5*std::sin(2*3.141592653589793*hz*n/sr));float v[]={a,opposite?-a:a};c.tick(v,2,s);check(std::isfinite(v[0]),"finite output");check(std::abs(v[0]-(opposite?-v[1]:v[1]))<1e-7,"stereo link");if(n>sr){e+=v[0]*v[0];++count;}}
 return std::sqrt(e/count);
}
void timingTests(){
 for(double sr:{44100.,48000.,96000.,192000.}){
  // A constant-energy alternating signal isolates the envelope from tone ripple.
  for(double attack:{.1,.3,1.,3.,10.,30.}){
   lindell::Settings s;s.knee=0;s.attackMs=attack;s.releaseSeconds=.1;lindell::Compressor c;c.prepare(sr,s);
   const double goal=lindell::Compressor::curve(20*std::log10(.5),s.threshold,s.ratio,0)*(1-std::exp(-1.));
   int n=0;for(;n<(int)sr&&c.gainReduction()<goal;++n){float v[]={n%2?.5f:-.5f};c.tick(v,1,s);}
   check(std::abs(n*1000/sr-attack)<.12+attack*.04,"measured attack time constant");
  }
  for(double release:{.1,.3,.6,1.2}){
   lindell::Settings s;s.knee=0;s.attackMs=.1;s.releaseSeconds=release;lindell::Compressor c;c.prepare(sr,s);
   for(int n=0;n<(int)(sr*.1);++n){float v[]={n%2?.5f:-.5f};c.tick(v,1,s);}
   const double start=c.gainReduction();for(int n=0;n<(int)(sr*release);++n){float v[]={0};c.tick(v,1,s);}
   check(std::abs(c.gainReduction()/start-std::exp(-1.))<.006,"measured release time constant");
  }
  auto recovery=[&](double duration){lindell::Settings s;s.attackMs=.1;s.releaseSeconds=0;s.knee=0;lindell::Compressor c;c.prepare(sr,s);
   for(int n=0;n<(int)(sr*duration);++n){float v[]={n%2?.5f:-.5f};c.tick(v,1,s);}double before=c.gainReduction();
   for(int n=0;n<(int)(sr*.2);++n){float v[]={0};c.tick(v,1,s);}return c.gainReduction()/before;};
  check(recovery(2)>recovery(.01)*2,"Auto releases sustained material more slowly than brief transients");
  // Preserve the old processor sample-for-sample when both controls are ORIG.
  lindell::Compressor current;legacy_reference::Compressor old;current.prepare(sr);old.prepare(sr);
  for(int n=0;n<40000;++n){lindell::Settings s;s.threshold=-12-(n/1000)%20;s.ratio=2+(n/2000)%8;
   legacy_reference::Settings legacy{s.threshold,s.ratio,s.output,s.hpf,s.mix,s.knee,s.bypass};
   float a[]={.7f*(float)std::sin(n*.1),.3f*(float)std::cos(n*.17)},b[]={a[0],a[1]};current.tick(a,2,s);old.tick(b,2,legacy);
   check(a[0]==b[0]&&a[1]==b[1],"ORIG is sample-exact with previous version");
  }
  lindell::Settings s;s.attackMs=10;s.releaseSeconds=0;double output=run(sr,1000,s);check(output<.22,"new default compresses");
 }
}
int main(){try{
 timingTests();
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
 std::cout<<"PASS: curve, five sample rates, stereo/antiphase, dry, mix, bypass, ratio, detector HPF, silence, measured attack/release, adaptive Auto, exact legacy compatibility\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
