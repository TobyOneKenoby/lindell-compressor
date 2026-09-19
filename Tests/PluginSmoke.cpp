#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
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
 auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());require(editor!=nullptr,"Editor missing");editor->setVisible(true);auto screenshot=editor->createComponentSnapshot(editor->getLocalBounds());juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile("Lindell-Rack-Preview.png");auto stream=file.createOutputStream();require(stream!=nullptr,"Preview file");juce::PNGImageFormat png;require(png.writeImageToStream(screenshot,*stream),"Preview encoding");stream.reset();editor.reset();p->releaseResources();std::cout<<"PASS: VST3 load, dry null, compression, state, editor render\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
