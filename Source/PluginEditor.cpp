#include "PluginEditor.h"
namespace {
constexpr float pi=juce::MathConstants<float>::pi;
const juce::Colour ink(0xffe1e6e5), muted(0xff91a9ba);
juce::Font font(float size,bool bold=false){return juce::Font(juce::FontOptions("Helvetica Neue",size,bold?juce::Font::bold:juce::Font::plain));}
void text(juce::Graphics& g,const juce::String& s,float x,float y,float w,float h,float size,juce::Colour colour=ink,bool bold=false){g.setColour(colour);g.setFont(font(size,bold));g.drawText(s,juce::Rectangle<float>(x,y,w,h),juce::Justification::centred);}
juce::Point<float> radial(juce::Point<float> c,float radius,float a){return {c.x+std::sin(a)*radius,c.y-std::cos(a)*radius};}
void led(juce::Graphics& g,float x,float y,juce::Colour colour,bool on,float radius=5){
 g.setColour(juce::Colour(0xff040b13));g.fillEllipse(x-radius-3,y-radius-3,2*radius+6,2*radius+6);
 g.setColour(juce::Colour(0xff80929c));g.drawEllipse(x-radius-2,y-radius-2,2*radius+4,2*radius+4,1);
 if(on){g.setGradientFill(juce::ColourGradient(colour.withAlpha(.35f),x,y,colour.withAlpha(0.f),x+radius*2.8f,y,true));g.fillEllipse(x-radius*2.8f,y-radius*2.8f,radius*5.6f,radius*5.6f);}
 g.setGradientFill(juce::ColourGradient(on?colour.brighter(.7f):colour.darker(1.5f),x-2,y-3,on?colour.darker(.2f):colour.darker(3.f),x+3,y+radius,false));g.fillEllipse(x-radius,y-radius,2*radius,2*radius);
 g.setColour(juce::Colours::white.withAlpha(on?.7f:.15f));g.fillEllipse(x-radius*.45f,y-radius*.65f,radius*.65f,radius*.5f);
}
void screw(juce::Graphics& g,float x,float y){
 g.setColour(juce::Colours::black.withAlpha(.7f));g.fillEllipse(x-9,y-8,20,20);
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd3d9d9),x-6,y-7,juce::Colour(0xff3a454d),x+7,y+8,false));g.fillEllipse(x-8,y-8,16,16);
 g.setColour(juce::Colour(0xffe7e7de).withAlpha(.6f));g.drawEllipse(x-7,y-7,14,14,.7f);
 g.setColour(juce::Colour(0xff17212a));g.drawLine(x-4,y+3,x+4,y-3,2.5f);
}
}
juce::Font RackLook::getLabelFont(juce::Label&){return font(14.f);}
void RackLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider){
 const float diameter=(float)std::min(w,h)-38.f;
 const juce::Point<float> c((float)x+(float)w*.5f,(float)y+(float)h*.5f);
 const float radius=diameter*.5f;
 const int intervals=slider.getProperties().contains("steps")?(int)slider.getProperties()["steps"]-1:30;
 for(int i=0;i<=intervals;++i){float a=start+(end-start)*(float)i/(float)intervals;bool major=intervals!=30||i%5==0;auto a1=radial(c,radius+9,a),a2=radial(c,radius+(major?16.f:12.f),a);g.setColour(ink.withAlpha(major?.9f:.45f));g.drawLine({a1,a2},major?1.5f:.8f);}
 juce::Rectangle<float> r(c.x-radius,c.y-radius,diameter,diameter);
 for(int i=8;i>0;--i){g.setColour(juce::Colours::black.withAlpha(.025f*(float)(9-i)));g.fillEllipse(r.expanded((float)i*.55f).translated(1,4));}
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xff75909f),r.getTopLeft(),juce::Colour(0xff03080d),r.getBottomRight(),false));g.fillEllipse(r);
 r.reduce(3,3);g.setColour(juce::Colour(0xff0c1620));g.fillEllipse(r);
 for(int i=0;i<96;++i){float a=2*pi*(float)i/96;g.setColour(i%2==0?juce::Colour(0xff78848c):juce::Colour(0xff242d34));g.drawLine({radial(c,radius-4,a),radial(c,radius-8,a)},1.2f);}
 r.reduce(6,6);
 juce::ColourGradient metal(juce::Colour(0xff89949c),r.getTopLeft(),juce::Colour(0xff6b7984),r.getBottomRight(),false);
 metal.addColour(.22,juce::Colour(0xffe5e8e6));metal.addColour(.47,juce::Colour(0xffb2bec3));metal.addColour(.64,juce::Colour(0xfff5f3eb));metal.addColour(.83,juce::Colour(0xffa1afb7));g.setGradientFill(metal);g.fillEllipse(r);
 {juce::Graphics::ScopedSaveState state(g);juce::Path circle;circle.addEllipse(r);g.reduceClipRegion(circle);for(float yy=r.getY();yy<r.getBottom();yy+=1.5f){g.setColour(juce::Colours::white.withAlpha(.055f));g.drawHorizontalLine((int)yy,r.getX(),r.getRight());}}
 g.setColour(juce::Colour(0xfff2f5ee).withAlpha(.7f));g.drawEllipse(r.reduced(.8f),.9f);
 float angle=start+pos*(end-start);
 {juce::Graphics::ScopedSaveState state(g);g.addTransform(juce::AffineTransform::rotation(angle,c.x,c.y));
 float width=diameter*.23f;juce::Rectangle<float> grip(c.x-width*.5f,c.y-r.getHeight()*.45f,width,r.getHeight()*.9f);
 g.setColour(juce::Colours::black.withAlpha(.4f));g.fillRoundedRectangle(grip.translated(2,2),4);
 juce::ColourGradient cap(juce::Colour(0xff7f8b93),grip.getTopLeft(),juce::Colour(0xffa5b0b6),grip.getTopRight(),false);cap.addColour(.23,juce::Colour(0xfff5f4ed));cap.addColour(.47,juce::Colour(0xffd6dcdb));cap.addColour(.8,juce::Colour(0xffb7c0c3));g.setGradientFill(cap);g.fillRoundedRectangle(grip,3);
 g.setColour(juce::Colour(0xffecede5));g.drawRoundedRectangle(grip.reduced(.6f),3,.8f);
 g.setColour(juce::Colour(0xff172a38));g.fillRoundedRectangle(c.x-1.6f,grip.getY()+4,3.2f,diameter*.17f,1.f);
 }
}
void RackLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down){
 float cx=(float)b.getWidth()*.5f;bool on=b.getToggleState();bool red=b.getName()=="red";
 led(g,cx,9,red?juce::Colour(0xfff26d42):juce::Colour(0xffb9e34c),on,4);
 juce::Rectangle<float> r(cx-13,24,26,26);
 g.setColour(juce::Colour(0xff030a12));g.fillEllipse(r.expanded(3));g.setColour(juce::Colour(0xff8495a1));g.drawEllipse(r.expanded(2),1);
 g.setGradientFill(juce::ColourGradient(juce::Colour(down?0xffa0aeb6:0xfff1eee4),r.getTopLeft(),juce::Colour(on?0xff98a6ac:0xffb9c3c4),r.getBottomRight(),false));g.fillEllipse(r);
 g.setColour(juce::Colours::white.withAlpha(hover?.85f:.45f));g.drawEllipse(r.reduced(1),1);
 text(g,b.getButtonText(),0,58,(float)b.getWidth(),18,10.5f,ink,true);
}
Editor::Editor(Processor& p):AudioProcessorEditor(p),processor(p){
 setLookAndFeel(&look);
 const char* ids[]={"threshold","ratio","output","hpf","mix","attack","release"};
 const double defaults[]={-18,4,0,30,100,5,5};
 for(size_t i=0;i<knobs.size();++i){auto& k=knobs[i];k.setSliderStyle(juce::Slider::RotaryVerticalDrag);k.setRotaryParameters(pi*1.25f,pi*2.75f,true);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,23);k.setDoubleClickReturnValue(true,defaults[i]);k.setColour(juce::Slider::textBoxTextColourId,ink);k.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff10283b));k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff446073).withAlpha(.45f));k.setTooltip("Drag to adjust. Hold Shift for fine control. Double-click to reset; click the value to type.");addAndMakeVisible(k);attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k);
 // Set after attachment so JUCE's parameter formatter cannot override display precision.
 if(i>=5){
  k.getProperties().set("steps",i==5?7:6);k.setTextBoxStyle(juce::Slider::TextBoxBelow,true,100,23);
  k.setTooltip(i==5?"Stepped attack. ORIG preserves the original timing; otherwise milliseconds.":"Stepped release in seconds. AUTO adapts recovery to sustained compression; ORIG preserves the original timing.");
 }
 k.textFromValueFunction=[i](double v){
 if(i>=5){const juce::StringArray attack{"ORIG","0.1 ms","0.3 ms","1 ms","3 ms","10 ms","30 ms"},release{"ORIG","0.1 s","0.3 s","0.6 s","1.2 s","AUTO"};const auto& choices=i==5?attack:release;return choices[juce::jlimit(0,choices.size()-1,(int)std::lround(v))];}
 if(std::abs(v)<.0001)v=0;return juce::String(v,i>=3?0:1)+(i==1?":1":i==3?" Hz":i==4?" %":" dB");};
 k.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue();};k.updateText();
 }
 addAndMakeVisible(bypass);addAndMakeVisible(knee);bypass.setName("red");
 bypassAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);kneeAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"knee",knee);
 const char* modes[]={"INPUT","OUTPUT","G.R."};
 for(int i=0;i<3;++i){auto& b=meterButtons[(size_t)i];b.setButtonText(modes[i]);b.setRadioGroupId(160);b.setClickingTogglesState(true);b.setToggleState(i==2,juce::dontSendNotification);if(i==2)b.setName("red");b.onClick=[this,i]{meterMode=i;repaint();};addAndMakeVisible(b);}
 setSize(1280,460);makePanel();startTimerHz(60);
}
Editor::~Editor(){stopTimer();setLookAndFeel(nullptr);}
void Editor::resized(){
 knobs[0].setBounds(225,78,164,186);knobs[1].setBounds(421,57,198,207);knobs[2].setBounds(651,78,164,186);
 knobs[3].setBounds(251,296,112,131);knobs[4].setBounds(677,296,112,131);
 knobs[5].setBounds(393,296,112,131);knobs[6].setBounds(535,296,112,131);
 knee.setBounds(62,324,64,81);bypass.setBounds(130,324,64,81);
 for(int i=0;i<3;++i)meterButtons[(size_t)i].setBounds(892+i*112,341,86,80);
}
void Editor::makePanel(){
 panel=juce::Image(juce::Image::RGB,1280,460,true);juce::Graphics g(panel);
 juce::ColourGradient blue(juce::Colour(0xff102535),0,0,juce::Colour(0xff173f5e),0,460,false);blue.addColour(.38,juce::Colour(0xff244c69));blue.addColour(.72,juce::Colour(0xff123550));g.setGradientFill(blue);g.fillAll();
 juce::Random random(160);
 for(int y=0;y<460;++y){g.setColour(juce::Colours::white.withAlpha(.008f+random.nextFloat()*.026f));g.drawHorizontalLine(y,0,1280);for(int n=0;n<8;++n){float x=random.nextFloat()*1280;g.setColour((n%2?juce::Colours::white:juce::Colours::black).withAlpha(.02f+random.nextFloat()*.035f));g.drawHorizontalLine(y,x,x+20+random.nextFloat()*200);}}
 g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack,640,220,juce::Colours::black.withAlpha(.45f),0,220,true));g.fillAll();
 g.setColour(juce::Colour(0xff030d17));g.drawRoundedRectangle({2,2,1276,456},12,4);
 g.setColour(juce::Colour(0xff91a4b0).withAlpha(.65f));g.drawRoundedRectangle({5,5,1270,450},9,1);
 g.setColour(juce::Colour(0xff051423));g.fillRect(47,8,2,444);g.fillRect(1231,8,2,444);
 for(int x:{12,1244})for(int y:{26,405}){g.setColour(juce::Colour(0xff020609));g.fillRoundedRectangle((float)x,(float)y,25,29,11);g.setColour(juce::Colour(0xff6c8290).withAlpha(.7f));g.drawRoundedRectangle((float)x,(float)y,25,29,11,1);}
 for(int x:{68,1212})for(int y:{27,434})screw(g,(float)x,(float)y);
 g.setColour(juce::Colour(0xffa3b9c5).withAlpha(.16f));g.drawLine(198,55,198,411,1);g.drawLine(841,55,841,411,1);
 // Screen-printed brand and engraved legends.
 text(g,"LINDELL",65,175,120,37,28,ink,true);text(g,"P L U G I N S",65,214,120,22,11,ink);
 g.setColour(juce::Colour(0xffd7b884));g.fillRect(88,151,73,3);
 text(g,"RACK",62,278,128,17,11,muted,true);text(g,"COMPRESSOR",62,296,128,18,11,muted,true);
 text(g,"V C A",85,413,85,14,9,juce::Colour(0xffd7b884),true);
 text(g,"THRESHOLD",216,40,182,23,13,ink,true);text(g,"COMPRESSION",415,30,210,23,13,ink,true);text(g,"OUTPUT GAIN",642,40,182,23,13,ink,true);
 text(g,"SIDECHAIN HPF",215,272,184,22,12,ink,true);text(g,"MIX",641,272,184,22,12,ink,true);
 text(g,"ATTACK",391,272,116,22,12,ink,true);text(g,"RELEASE",533,272,116,22,12,ink,true);
 text(g,"ms",407,432,84,14,9,muted);text(g,"s / AUTO",549,432,84,14,9,muted);
 text(g,"DETECTOR ONLY",216,432,182,14,9,muted);text(g,"DRY / WET",642,432,182,14,9,muted);
 text(g,"30",225,370,23,14,9);text(g,"300",365,370,30,14,9);text(g,"0",651,370,23,14,9);text(g,"100",791,370,30,14,9);
 text(g,"-60",210,218,30,15,10);text(g,"0",376,218,30,15,10);text(g,"-30",292,65,30,15,10);
 text(g,"1:1",398,219,36,15,10);text(g,"20:1",607,219,39,15,10);text(g,"4:1",502,58,36,15,10);
 text(g,"-20",636,218,30,15,10);text(g,"+20",802,218,33,15,10);text(g,"0",718,65,30,15,10);
 text(g,"METER SELECTION",904,310,275,22,11,muted,true);
 text(g,"ANALOG DYNAMICS",894,28,300,18,11,muted,true);
}
void Editor::timerCallback(){
 float target=1.f;
 if(meterMode==2)target=juce::jlimit(0.f,1.f,1.f-processor.grDb.load()/30.f);
 else {float db=(meterMode==0?processor.inputVuDb.load():processor.outputVuDb.load())+18.f;target=juce::jlimit(0.f,1.f,(std::pow(10.f,db/20.f)-.1f)/1.31254f);}
 needle+=(target-needle)*.16f;
 repaint(855,36,365,268);
}
void Editor::drawMeter(juce::Graphics& g){
 const juce::Rectangle<float> bezel(863,61,351,235), face(879,78,319,200);
 g.setColour(juce::Colours::black.withAlpha(.5f));g.fillRoundedRectangle(bezel.translated(2,5).expanded(3),20);
 juce::ColourGradient rim(juce::Colour(0xff7792a3),bezel.getTopLeft(),juce::Colour(0xff071524),bezel.getBottomRight(),false);rim.addColour(.15,juce::Colour(0xff29465f));rim.addColour(.5,juce::Colour(0xff0a1b2a));rim.addColour(.85,juce::Colour(0xff1c3549));g.setGradientFill(rim);g.fillRoundedRectangle(bezel,18);
 g.setColour(juce::Colour(0xff02070c));g.fillRoundedRectangle(face.expanded(4),12);
 juce::ColourGradient paper(juce::Colour(0xffffefc8),1038,160,juce::Colour(0xffb99561),879,78,true);paper.addColour(.7,juce::Colour(0xfff5deb0));g.setGradientFill(paper);g.fillRoundedRectangle(face,9);
 juce::Graphics::ScopedSaveState clip(g);juce::Path clipPath;clipPath.addRoundedRectangle(face,9);g.reduceClipRegion(clipPath);
 const juce::Point<float> pivot(1038.5f,292.f);
 const float radius=165.f;const float lo=-.84f, hi=.84f;
 const juce::Colour black(0xff302b25),red(0xffa33f2f);
 auto arc=[&](float a,float b,float r,juce::Colour colour,float width){juce::Path p;p.addCentredArc(pivot.x,pivot.y,r,r,0,a,b,true);g.setColour(colour);g.strokePath(p,juce::PathStrokeType(width));};
 arc(lo,hi,radius,black,1.1f);
 if(meterMode!=2)arc(lo+(hi-lo)*.686f,hi,radius,red,3.5f);
 for(int i=0;i<=40;++i){float f=(float)i/40;float a=lo+(hi-lo)*f;bool major=i%5==0;g.setColour(meterMode!=2&&f>.686f?red:black);g.drawLine({radial(pivot,radius-2,a),radial(pivot,radius+(major?10.f:5.f),a)},major?1.2f:.65f);}
 if(meterMode==2){const char* labels[]={"30","25","20","15","10","5","0"};for(int i=0;i<7;++i){auto p=radial(pivot,radius+24,lo+(hi-lo)*(float)i/6);text(g,labels[i],p.x-15,p.y-8,30,16,12,black,true);}}
 else {const float values[]={-20,-10,-7,-5,-3,0,3};for(float v:values){float f=(std::pow(10.f,v/20.f)-.1f)/1.31254f;auto p=radial(pivot,radius+24,lo+(hi-lo)*f);text(g,v>0?"+3":juce::String((int)v),p.x-14,p.y-8,28,16,12,v>=0?red:black,true);}}
 text(g,meterMode==2?"GAIN REDUCTION":"VU",929,184,219,26,meterMode==2?12.f:22.f,black,true);
 text(g,"LINDELL",949,216,179,24,20,red,true);text(g,meterMode==2?"DECIBELS":"0 VU = -18 dBFS",954,244,170,15,9,black);
 float angle=lo+(hi-lo)*needle;auto end=radial(pivot,radius+9,angle);
 g.setColour(juce::Colours::black.withAlpha(.18f));g.drawLine(pivot.x+3,pivot.y+3,end.x+3,end.y+3,3);
 g.setColour(juce::Colour(0xff392d24));g.drawLine({pivot,end},1.8f);g.setColour(red);g.drawLine({radial(pivot,radius-26,angle),end},1.7f);
 juce::Path reflection;reflection.startNewSubPath(879,78);reflection.lineTo(1198,78);reflection.lineTo(879,155);reflection.closeSubPath();g.setColour(juce::Colours::white.withAlpha(.12f));g.fillPath(reflection);
 // Independent peak lamp remains visible in every meter mode.
}
void Editor::paint(juce::Graphics& g){
 g.drawImageAt(panel,0,0);drawMeter(g);
 led(g,1173,48,juce::Colour(0xfff35a37),processor.outputDb.load()>-.5f,3.5f);text(g,"PEAK",1111,39,43,17,9,muted);
}
