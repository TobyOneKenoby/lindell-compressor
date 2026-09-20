#pragma once
#include <algorithm>
#include <cmath>
namespace legacy_reference {
struct Settings { double threshold=-18, ratio=4, output=0, hpf=30, mix=100, knee=1, bypass=0; };
class Compressor {
 double rate=48000, smooth=0, rmsCoef=0, power=0, reduction=0;
 double x[2]{}, y[2]{}; Settings current;
 public:
 void prepare(double sr, Settings s={}) { rate=sr; smooth=std::exp(-1/(.02*sr)); rmsCoef=std::exp(-1/(.01*sr)); power=reduction=0; x[0]=x[1]=y[0]=y[1]=0; current=s; }
 static double curve(double db,double threshold,double ratio,double width) {
  double over=db-threshold, slope=1-1/ratio;
  if(width>0 && over>-width/2 && over<width/2) return slope*(over+width/2)*(over+width/2)/(2*width);
  return slope*std::max(0.0,over);
 }
 double gainReduction() const { return reduction; }
 void tick(float* samples,int channels,const Settings& target) {
  auto ramp=[&](double& v,double t){v=t+smooth*(v-t); if(std::abs(v-t)<1e-9)v=t;};
  ramp(current.threshold,target.threshold);ramp(current.ratio,target.ratio);ramp(current.output,target.output);
  ramp(current.hpf,target.hpf);ramp(current.mix,target.mix);ramp(current.knee,target.knee);ramp(current.bypass,target.bypass);
  // Bilinear one-pole HPF in detector only, continuous state during automation.
  const double k=std::tan(3.141592653589793*current.hpf/rate), b=1/(1+k), a=(1-k)/(1+k);
  double energy=0;
  for(int c=0;c<channels;++c){ double v=b*(samples[c]-x[c])+a*y[c];x[c]=samples[c];y[c]=v;energy=std::max(energy,v*v); }
  // Max channel power links stereo without cancellation of out-of-phase material.
  power=rmsCoef*power+(1-rmsCoef)*energy;
  double wanted=curve(10*std::log10(std::max(1e-20,power)),current.threshold,current.ratio,6*current.knee);
  // Behavioral prototype: faster response to large excursions; dual-speed recovery.
  const double seconds=wanted>reduction ? .001+.009/(1+std::max(0.0,wanted-reduction)) : .06+.24*std::min(1.0,reduction/20);
  const double coefficient=std::exp(-1/(seconds*rate));
  reduction=wanted+coefficient*(reduction-wanted);
  const double gain=std::pow(10.,(current.output-reduction)/20);
  const double wet=current.mix*.01*(1-current.bypass);
  for(int c=0;c<channels;++c) samples[c]=static_cast<float>(samples[c]*(1+wet*(gain-1)));
 }
};
}

