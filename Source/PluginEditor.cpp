#include "PluginEditor.h"
void RackLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& s){
 auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(13);auto size=std::min(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(size,size);auto c=r.getCentre();
 for(int i=0;i<=10;++i){float a=start+(end-start)*(float)i/10;g.setColour(juce::Colour(0xffc5c8c6));g.drawLine(c.x+std::sin(a)*(size/2+5),c.y-std::cos(a)*(size/2+5),c.x+std::sin(a)*(size/2+9),c.y-std::cos(a)*(size/2+9),1);}
 g.setColour(juce::Colours::black.withAlpha(.6f));g.fillEllipse(r.translated(2,4));
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xff686b6a),r.getTopLeft(),juce::Colour(0xff101212),r.getBottomRight(),false));g.fillEllipse(r);r.reduce(5,5);
 auto colour=s.findColour(juce::Slider::rotarySliderFillColourId);g.setGradientFill(juce::ColourGradient(colour.brighter(.3f),r.getTopLeft(),colour.darker(.4f),r.getBottomRight(),false));g.fillEllipse(r);
 float a=start+pos*(end-start);g.setColour(juce::Colour(0xfff4eee0));g.drawLine(c.x+std::sin(a)*size*.12f,c.y-std::cos(a)*size*.12f,c.x+std::sin(a)*size*.37f,c.y-std::cos(a)*size*.37f,3);
}
Editor::Editor(Processor& p):AudioProcessorEditor(p),processor(p){
 setLookAndFeel(&look);setSize(1200,240);
 const char* ids[]={"threshold","ratio","output","hpf","mix"};const char* units[]={" dB",":1"," dB"," Hz"," %"};
 const juce::uint32 colours[]={0xffc8443b,0xff327ba2,0xff448762,0xffb6924d,0xffa6a9ab};
 const double defaults[]={-18,4,0,30,100};
 for(size_t i=0;i<knobs.size();++i){auto& k=knobs[i];k.setSliderStyle(juce::Slider::RotaryVerticalDrag);k.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,92,22);k.setTextValueSuffix(units[i]);k.setNumDecimalPlacesToDisplay(i==3||i==4?0:1);k.setDoubleClickReturnValue(true,defaults[i]);k.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(colours[i]));k.setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe3e5e2));k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);k.setTooltip("Drag to adjust; hold Shift for fine control. Double-click to reset.");addAndMakeVisible(k);attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k);}
 addAndMakeVisible(bypass);addAndMakeVisible(knee);bypassAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);kneeAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"knee",knee);startTimerHz(30);
}
Editor::~Editor(){stopTimer();setLookAndFeel(nullptr);}
void Editor::resized(){for(int i=0;i<5;++i)knobs[(size_t)i].setBounds(212+i*116,46,108,146);bypass.setBounds(55,155,130,24);knee.setBounds(216,204,145,23);}
void Editor::timerCallback(){if(++frame%6==0){in=processor.inputDb.load();out=processor.outputDb.load();gr=processor.grDb.load();}repaint(808,42,350,180);}
void Editor::paint(juce::Graphics& g){
 auto bounds=getLocalBounds().toFloat();g.fillAll(juce::Colour(0xff0b0d0e));g.setGradientFill(juce::ColourGradient(juce::Colour(0xff363839),0,0,juce::Colour(0xff1b1d1e),0,240,false));g.fillRoundedRectangle(bounds.reduced(3),7);
 g.setColour(juce::Colour(0xff555859));g.drawRoundedRectangle(bounds.reduced(4),6,1);
 for(int yy=8;yy<235;yy+=3){g.setColour(juce::Colours::white.withAlpha(.013f));g.drawHorizontalLine(yy,5,1195);}
 for(int xx:{16,1166})for(int yy:{22,196}){g.setColour(juce::Colour(0xff07090b));g.fillRoundedRectangle((float)xx,(float)yy,18,22,8);}
 for(int xx:{45,1146})for(int yy:{18,212}){g.setColour(juce::Colour(0xff111315));g.fillEllipse((float)xx,(float)yy,10,10);g.setColour(juce::Colour(0xff626463));g.drawLine((float)xx+3,(float)yy+5,(float)xx+7,(float)yy+5,1);}
 g.setColour(juce::Colour(0xff3486a6));for(int xx:{197,802})g.fillRect(xx,38,2,163);
 g.setColour(juce::Colour(0xffebeeeb));g.setFont(26.f);g.drawText("Lindell",54,52,140,32,juce::Justification::left);g.setFont(17.f);g.drawText("Plugins",55,83,140,24,juce::Justification::left);g.setColour(juce::Colour(0xffa4acac));g.setFont(11.f);g.drawText("RACK COMPRESSOR",55,120,140,18,juce::Justification::left);
 const char* labels[]={"THRESHOLD","RATIO","OUTPUT GAIN","SC HIGH PASS","MIX"};g.setFont(12.f);g.setColour(juce::Colour(0xffe2e4df));for(int i=0;i<5;++i)g.drawText(labels[i],210+i*116,26,112,18,juce::Justification::centred);
 g.setFont(10.f);g.setColour(juce::Colour(0xff9da7a9));g.drawText("30–300 Hz · DETECTOR ONLY",536,201,165,18,juce::Justification::centred);g.drawText("DRY / WET",681,201,104,18,juce::Justification::centred);
 const float vals[]={processor.inputDb.load(),processor.outputDb.load(),processor.grDb.load()};const char* names[]={"IN","OUT","GR"};
 for(int row=0;row<3;++row){int yy=62+row*43;g.setColour(juce::Colour(0xffd1d5d2));g.setFont(10.f);g.drawText(names[row],818,yy-4,29,18,juce::Justification::left);
 for(int j=0;j<20;++j){float t=row==2?(float)(j+1)*1.5f:-57.f+3.f*(float)j;bool lit=vals[row]>=t;auto col=row==2?juce::Colour(0xffed4a3e):(j>=17?juce::Colour(0xfff25045):juce::Colour(0xffdfcb4a));g.setColour(lit?col:col.darker(2.8f));g.fillEllipse(851.f+14.f*(float)j,(float)yy,8,8);}
 }
 g.setFont(10.f);g.setColour(juce::Colour(0xff939f9f));g.drawText("−60             −36             −18              0 dBFS",850,39,282,16,juce::Justification::centred);g.drawText("GAIN REDUCTION   0 → 30 dB",850,170,282,16,juce::Justification::centred);
 g.setFont(12.f);g.setColour(juce::Colour(0xffd8dfdc));g.drawText("IN "+juce::String(in,1)+"   OUT "+juce::String(out,1)+"   GR "+juce::String(gr,1),818,195,320,22,juce::Justification::centred);
}
