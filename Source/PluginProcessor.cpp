#include "PluginProcessor.h"
#include "PluginEditor.h"
static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 auto add=[&](const char* id,const char* name,float lo,float hi,float def,float skew){
  p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(lo,hi,.01f,skew),def));};
 add("threshold","Threshold",-60,0,-18,1);add("ratio","Ratio",1,20,4,.5f);add("output","Output",-20,20,0,1);
 add("hpf","Sidechain HPF",30,300,30,.5f);add("mix","Mix",0,100,100,1);
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"knee",1},"Soft knee",true));
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
 p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"attack",1},"Attack",juce::StringArray{"ORIG","0.1 ms","0.3 ms","1 ms","3 ms","10 ms","30 ms"},5));
 p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"release",1},"Release",juce::StringArray{"ORIG","0.1 s","0.3 s","0.6 s","1.2 s","AUTO"},5));return p;
}
Processor::Processor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"LindellRackState",layout()){
 const char* ids[]={"threshold","ratio","output","hpf","mix","knee","bypass","attack","release"};for(int i=0;i<9;++i)values[(size_t)i]=state.getRawParameterValue(ids[i]);
}
lindell::Settings Processor::settings()const{
 static constexpr double attacks[]={-1,.1,.3,1,3,10,30}, releases[]={-1,.1,.3,.6,1.2,0};
 return {values[0]->load(),values[1]->load(),values[2]->load(),values[3]->load(),values[4]->load(),values[5]->load(),values[6]->load(),attacks[juce::jlimit(0,6,(int)values[7]->load())],releases[juce::jlimit(0,5,(int)values[8]->load())]};}
void Processor::prepareToPlay(double sr,int){core.prepare(sr,settings());inMeter=outMeter=0;inputPower=outputPower=0;inputVuDb=outputVuDb=-100;meterDecay=std::exp(-1/(.3*sr));setLatencySamples(0);}
bool Processor::isBusesLayoutSupported(const BusesLayout& l)const{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
void Processor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&){
 juce::ScopedNoDenormals guard; const auto s=settings();const int channels=b.getNumChannels();
 for(int n=0;n<b.getNumSamples();++n){float v[2]{};for(int c=0;c<channels;++c){v[c]=b.getSample(c,n);}
 float ip=0;for(int c=0;c<channels;++c)ip=std::max(ip,std::abs(v[c]));core.tick(v,channels,s);
 float op=0;for(int c=0;c<channels;++c){b.setSample(c,n,v[c]);op=std::max(op,std::abs(v[c]));}
 inputPower=meterDecay*inputPower+(1-meterDecay)*(double)ip*ip;outputPower=meterDecay*outputPower+(1-meterDecay)*(double)op*op;
 inMeter=std::max(ip,static_cast<float>(inMeter*meterDecay));outMeter=std::max(op,static_cast<float>(outMeter*meterDecay));}
 inputVuDb.store(static_cast<float>(10*std::log10(std::max(1e-10,inputPower))));outputVuDb.store(static_cast<float>(10*std::log10(std::max(1e-10,outputPower))));
 inputDb.store(juce::Decibels::gainToDecibels(inMeter,-100.f));outputDb.store(juce::Decibels::gainToDecibels(outMeter,-100.f));grDb.store(s.bypass>.5?0:static_cast<float>(core.gainReduction()));
}
void Processor::getStateInformation(juce::MemoryBlock& b){if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,b);}
void Processor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n))if(xml->hasTagName(state.state.getType())){
 auto restored=juce::ValueTree::fromXml(*xml);
 // Older sessions have no timing parameters. Explicit ORIG migration avoids
 // changing their sound, even if another preset was loaded first.
 for(const char* id:{"attack","release"})if(!restored.getChildWithProperty("id",id).isValid()){
  juce::ValueTree parameter("PARAM");parameter.setProperty("id",id,nullptr);parameter.setProperty("value",0.f,nullptr);restored.addChild(parameter,-1,nullptr);
 }
 state.replaceState(restored);
}}
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Processor;}
