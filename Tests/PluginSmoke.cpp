#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include "../Source/PluginProcessor.h"
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;try{
 require(argc==2,"Pass VST3 bundle path");juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,argv[1]);require(types.size()==1,"Exactly one plugin expected");
 juce::AudioPluginFormatManager manager;manager.addFormat(new juce::VST3PluginFormat);juce::String error;auto p=manager.createPluginInstance(*types[0],48000,256,error);require(p!=nullptr,"VST3 instantiation failed");
 auto set=[&](const juce::String& name,float value){for(auto* parameter:p->getParameters())if(parameter->getName(100)==name){parameter->setValueNotifyingHost(value);return;}throw std::runtime_error("Missing parameter");};
 require(p->getTotalNumInputChannels()==2&&p->getTotalNumOutputChannels()==2,"Stereo buses");set("Mix",0);p->prepareToPlay(48000,256);juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;
 for(int block=0;block<10;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)b.setSample(ch,i,.25f);p->processBlock(b,midi);for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)require(b.getSample(ch,i)==.25f,"Dry must null");}
 set("Mix",1);double power=0;for(int block=0;block<500;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)b.setSample(ch,i,.5f*std::sin(juce::MathConstants<float>::twoPi*1000*(float)(block*256+i)/48000));p->processBlock(b,midi);if(block>400)for(int i=0;i<256;++i)power+=b.getSample(0,i)*b.getSample(0,i);}
 require(power/(99*256)<.05,"Compression must reduce power");juce::MemoryBlock state;p->getStateInformation(state);require(state.getSize()>0,"State missing");p->setStateInformation(state.getData(),(int)state.getSize());
 set("Red mode",1);set("Attack",2.f/6);set("Release",3.f/5);p->getStateInformation(state);set("Red mode",0);set("Attack",1);set("Release",0);p->setStateInformation(state.getData(),(int)state.getSize());
 for(auto* parameter:p->getParameters()){
  if(parameter->getName(100)=="Red mode")require(parameter->getValue()==1,"Red mode state recall");
  if(parameter->getName(100)=="Attack")require(std::abs(parameter->getValue()-2.f/6)<.001f,"Attack state recall");
  if(parameter->getName(100)=="Release")require(std::abs(parameter->getValue()-3.f/5)<.001f,"Release state recall");
 }
 Processor migration;auto oldState=migration.state.copyState();
 oldState.removeChild(oldState.getChildWithProperty("id","model"),nullptr);
 migration.state.getParameter("model")->setValueNotifyingHost(1);
 oldState.removeChild(oldState.getChildWithProperty("id","attack"),nullptr);oldState.removeChild(oldState.getChildWithProperty("id","release"),nullptr);
 juce::MemoryBlock legacyState;auto legacyXml=oldState.createXml();juce::AudioProcessor::copyXmlToBinary(*legacyXml,legacyState);
 migration.setStateInformation(legacyState.getData(),(int)legacyState.getSize());
 require(migration.settings().model==0,"Old project migrates to Blue even after Red preset");
 require(migration.settings().attackMs==-1&&migration.settings().releaseSeconds==-1,"Old project retains original timing");
 auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());require(editor!=nullptr,"Editor missing");editor->setVisible(true);
 // VST3 native NSView embedding is opaque to host-side JUCE snapshots.
 // Render the identical Editor source directly for visual verification.
 Processor previewProcessor;
 auto preview=std::unique_ptr<juce::AudioProcessorEditor>(previewProcessor.createEditor());
 preview->setVisible(true);
 juce::Image screenshot(juce::Image::ARGB,preview->getWidth(),preview->getHeight(),true,juce::SoftwareImageType());
 {juce::Graphics graphics(screenshot);preview->paintEntireComponent(graphics,true);}
 require(screenshot.getPixelAt(1038,160).getBrightness()>.4f,"Meter preview must not be blank");
 juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile("Lindell-Rack-Preview.png");auto stream=file.createOutputStream();require(stream!=nullptr,"Preview file");juce::PNGImageFormat png;require(png.writeImageToStream(screenshot,*stream),"Preview encoding");stream.reset();
 preview.reset();previewProcessor.state.getParameter("model")->setValueNotifyingHost(1);
 preview.reset(previewProcessor.createEditor());preview->setVisible(true);
 {juce::Graphics graphics(screenshot);preview->paintEntireComponent(graphics,true);}
 const auto colour=screenshot.getPixelAt(800,30);require(colour.getRed()>colour.getBlue(),"Red panel follows restored mode");
 file=juce::File::getCurrentWorkingDirectory().getChildFile("Lindell-Red-Preview.png");stream=file.createOutputStream();require(stream!=nullptr&&png.writeImageToStream(screenshot,*stream),"Red preview encoding");stream.reset();
 // Exercise RED in the actual loaded VST3 as well as the dependency-free core.
 set("Mix",1);set("Red mode",1);p->prepareToPlay(48000,256);power=0;
 for(int block=0;block<500;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)b.setSample(ch,i,.5f*std::sin(juce::MathConstants<float>::twoPi*1000*(float)(block*256+i)/48000));p->processBlock(b,midi);if(block>400)for(int i=0;i<256;++i)power+=b.getSample(0,i)*b.getSample(0,i);}
 require(power/(99*256)<.05&&power>0,"Red VST3 processes compressed audio");
 editor.reset();p->releaseResources();std::cout<<"PASS: VST3 load, dry null, compression, state, timing recall, old-project migration, editor render\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
