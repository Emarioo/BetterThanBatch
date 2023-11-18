#include "Engone/UIModule.h"
#include "stb/stb_image.h"

// You should define this when compiling
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#undef APIENTRY // collision with minwindef

#include "Engone/VertexShader.glsl"

#include "Engone/Asserts.h"

// #include "Engone/Window.h"

#define GL_CHECK_UI()  {int err = glGetError();if(err) {printf("UIMod GLError: %d %s\n",err,(const char*)glewGetErrorString(err));}}


namespace engone{
    
bool UIArray::reserve(int newMax){
    if(!data){
        if(newMax!=0){
            data = malloc(newMax*m_typeSize);
            if(!data) return false;
            max = newMax;
            count = 0;   
        }
    }else{
        if(newMax==0){
            free(data);
            data = 0;
            max = 0;
            count = 0;
        }else{
            void* temp = realloc(data,newMax*m_typeSize);
            if(!temp) return false;
            data = temp;
            max = newMax;
            if(newMax<count)
                count = newMax;   
        }
    }
    return true;
}

float UIModule::getWidthOfText(UIText* uitext){
    // We don't assert because we allow the frame to load the next few frames.
    // If the font never loads then we have a problem. It will probably be noticable though.
    if(fonts.size()<=uitext->fontId) return 0;
    
    UIFont& font = fonts[uitext->fontId];
    return font.charWidth * uitext->getLength() * uitext->h / font.cellH;    
}
bool UIModule::inside(UIBox* box, float x, float y){
    return box->x<x&& box->x + box->w>x && box->y<y&& box->y + box->h>y;
}
bool UIModule::inside(UIText* box, float x, float y) {
    float w = getWidthOfText(box);
    return box->x<x&& box->x + w>x && box->y<y&& box->y + box->h>y;
}
#ifndef DISABLE_USER_INPUT  
void UIModule::edit(UIText* text, bool stopEditWithEnter) {
    text->editing = true;
    int length = text->text.length();
    if (text->cursorIndex < 0 || text->cursorIndex > length) text->cursorIndex = length;
    
    uint32_t chr = 0;
    if (IsKeyDown(GLFW_KEY_LEFT_CONTROL) && IsKeyPressed(GLFW_KEY_V)) {
        std::string tmp = PollClipboard();
        //str.insert(str.begin() + at, tmp);
        text->text.insert(text->cursorIndex, tmp);
        text->cursorIndex += (int)tmp.length();
        return;
    }
    while (chr = PollChar()) {
        length = text->text.length();
        if (chr == GLFW_KEY_BACKSPACE) {
            if (length > 0 && text->cursorIndex > 0) {
                text->text = text->text.substr(0, text->cursorIndex - 1) + text->text.substr(text->cursorIndex);
                text->cursorIndex--;
            }
        } else if (chr == GLFW_KEY_ENTER) {
            if (stopEditWithEnter) {
                text->editing = false;
            } else {
                text->text.insert(text->text.begin() + text->cursorIndex, '\n');
                text->cursorIndex++;
            }
        } else if (chr == GLFW_KEY_DELETE) {
            if ((int)text->text.length() > text->cursorIndex) {
                text->text = text->text.substr(0, text->cursorIndex) + text->text.substr(text->cursorIndex + 1);
            }
        } else if (chr == GLFW_KEY_LEFT) {
            if (text->cursorIndex > 0)
                text->cursorIndex--;
        } else if (chr == GLFW_KEY_RIGHT) {
            if (text->cursorIndex < (int)text->text.length())
                text->cursorIndex++;
        } else {
            text->text.insert(text->text.begin() + text->cursorIndex, chr);
            text->cursorIndex++;
        }
    }
}
bool UIModule::clicked(UIBox* box, int mouseKey) {
    if (IsKeyPressed(mouseKey)) {
        float x = GetMouseX();
        float y = GetMouseY();
        if (inside(box, x, y)) {
            return true;
        }
    }
    return 0;
}
bool UIModule::clicked(UIText* text, int mouseKey) {
    if (IsKeyPressed(mouseKey)) {
        float x = GetMouseX();
        float y = GetMouseY();
        if (inside(text, x, y)) {
            // set cursor
            if(fonts.size()<=text->fontId) return false;
            
            float wid = fonts[text->fontId].charWidth;
            text->cursorIndex = ((x - text->x) / wid + 0.5f);
            return true;
        }
    }
    return false;
}
bool UIModule::hover(UIBox* box) {
    float x = GetMouseX();
    float y = GetMouseY();
    if (inside(box, x, y)) {
        return true;
    }
    return false;
}
bool UIModule::hover(UIText* box) {
    float x = GetMouseX();
    float y = GetMouseY();
    if (inside(box, x, y)) {
        return true;
    }
    return false;
}
#endif

unsigned int CompileUIShader(unsigned int type, const char* str){
    unsigned int id = glCreateShader(type);
    
    int length = strlen(str);
    
    glShaderSource(id, 1, &str, &length);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        //error = 2;
        int msgSize; // null terminate is included
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &msgSize);

        char* msg = (char*)new char(msgSize);// a bit extra for when memory is moved to fit in actual error line
        memset(msg, 0, msgSize);

        int length=0;
        glGetShaderInfoLog(id, msgSize, &length, msg); // length is alter and does no longer include null term char

        printf("%s\n",msg);

        glDeleteShader(id);

        delete msg;

        return 0;
    }
    return id;
}
unsigned int CreateUIShader(){
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileUIShader(GL_VERTEX_SHADER, vs_VertexShaderGLSL);
    unsigned int fs = CompileUIShader(GL_FRAGMENT_SHADER, fs_VertexShaderGLSL);

    if (vs == 0 || fs == 0) {
        // failed
        if(vs)
            glDeleteShader(vs);
        if(fs)
            glDeleteShader(fs);

        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    //int eh=0;
    //glGetProgramiv(program, GL_VALIDATE_STATUS, &eh);
    //log::out << "Error? " << eh<<"\n";

    glDeleteShader(vs);
    glDeleteShader(fs);
    
    return program;
}
UIFrameArray::UIFrameArray(uint32 typeSize, uint32 valuesPerFrame) :
    m_typeSize(typeSize), m_valuesPerFrame(valuesPerFrame) {
    //uint32 off = valuesPerFrame & 63;
    //if(off!=0)
    //	m_valuesPerFrame += 64 - off;
    uint32 off = valuesPerFrame & 7;
    if (off != 0)
        m_valuesPerFrame += 8 - off;
}
void UIFrameArray::cleanup() {
    for (int i = 0; i < m_frames.max; i++) {
        Frame& frame = *((Frame*)m_frames.data + i);
        // Todo: if using UIText, string is not cleaned up
        // if (frame.array.max != 0) {
        // 	for (int j = 0; j < m_valuesPerFrame; j++) {
        // 		bool yes = frame.getBool(j);
        // 		if (yes) {
        // 			Value* ptr = frame.getValue(j, m_typeSize, m_valuesPerFrame);
        // 			ptr->~Value();
        // 		}
        // 	}
        // }
        frame.array.reserve(0);
    }
    m_frames.reserve(0);
}
// Returns -1 if something failed
uint32 UIFrameArray::add(void* value, void** outPtr) {
    Assert(m_valuesPerFrame != 0);
    //if(m_valuesPerFrame !=8)
    //	log::out << log::Disable;

    //log::out << "FA : ADD "<<m_valuesPerFrame<<"\n";
    // Find available frame
    int frameIndex = -1;
    for (int i = 0; i < m_frames.max; i++) {
        Frame& frame = *((Frame*)m_frames.data + i);
        //log::out << "FA : " << frame.count << " != " << m_valuesPerFrame<< "\n";
        if (frame.count != m_valuesPerFrame) {
            frameIndex = i;
            //log::out << "FA : found frame "<<i<<" with "<< frame.count << " objects\n";
            break;
        }
    }
    // Create new frame if non found
    if (frameIndex == -1) {
        int initialMax = m_frames.max;
        bool yes = m_frames.reserve(m_frames.max * 1.5 + 1);
        Assert(yes);
        // if (!yes) {
        //     //log::out >> log::Disable;
        //     log::out << log::RED << "FrameArray : failed resize frames\n";
        //     return -1;
        // }
        // memset((Frame*)m_frames.data + initialMax, 0, (m_frames.max - initialMax) * sizeof(Frame));
        frameIndex = initialMax;
        for (int i = initialMax; i < m_frames.max; i++) {
            *((Frame*)m_frames.data + i) = {};
        }
        //log::out << "FA : Create frame " << frameIndex << "\n";
    }

    // Insert value into frame
    Frame& frame = *((Frame*)m_frames.data + frameIndex);
    if (frame.array.max == 0) {
        bool yes = frame.array.reserve(m_valuesPerFrame + m_valuesPerFrame * m_typeSize);
        //bool yes = frame.memory.resize(m_valuesPerFrame/8+ m_valuesPerFrame * sizeof(Value));
        Assert(yes);
        // if (!yes) {
        //     log::out << log::RED << "FrameArray : failed resize frame\n";
        //     //log::out >> log::Disable; 
        //     return -1;
        // }

        //memset(frame.memory.data,0,m_valuesPerFrame/8);
        memset(frame.array.data,0,m_valuesPerFrame);
        //log::out << "FA : Reserve values\n";
    }

    // Find empty slot
    int valueIndex = -1;
    for (int i = 0; i < m_valuesPerFrame; i++) {
        bool yes = frame.getBool(i);
        if (!yes) {
            valueIndex = i;
            //log::out << "FA : found spot " << i << "\n";
            break;
        } else {
            //log::out << "FA : checked spot " << i <<"\n";
        }
    }
    Assert(valueIndex!=-1);
    // if (valueIndex == -1) {
    //     log::out << log::RED << "FrameArray : Impossible error adding value in frame " << frameIndex << "\n";
    //     //log::out >> log::Disable;
    //     return -1;
    // }
    frame.count++;
    //log::out << "Frame : New object count " << frame.count << "\n";
    // m_valueCount++;
    frame.setBool(valueIndex, true);
    void* ptr = frame.getValue(valueIndex, m_typeSize, m_valuesPerFrame);
    //new(ptr)Value(value);
    if (value)
        memcpy(ptr,value,m_typeSize);
    else
        memset(ptr, 1, m_typeSize);
    //log::out >> log::Disable;
    if (outPtr)
        *outPtr = ptr;
    //log::out << "FA:Add " << valueIndex << "\n";
    return frameIndex * m_valuesPerFrame + valueIndex;
}
void* UIFrameArray::get(uint32 index) {
    uint32 frameIndex = index / m_valuesPerFrame;
    uint32 valueIndex = index % m_valuesPerFrame;

    if (frameIndex >= m_frames.max)
        return nullptr;

    Frame& frame = *((Frame*)m_frames.data + frameIndex);

    if (valueIndex >= frame.array.max)
        return nullptr;

    bool yes = frame.getBool(valueIndex);
    if (!yes) // Check if slot is empty
        return nullptr;

    void* ptr = frame.getValue(valueIndex, m_typeSize, m_valuesPerFrame);
    return ptr;
}
void UIFrameArray::remove(uint32 index) {
    uint32 frameIndex = index / m_valuesPerFrame;
    uint32 valueIndex = index % m_valuesPerFrame;

    if (frameIndex >= m_frames.max)
        return;

    Frame& frame = *((Frame*)m_frames.data + frameIndex);

    if (frame.array.max == 0)
        return;
    bool yes = frame.getBool(valueIndex);
    if (!yes)
        return;

    frame.count--;
    frame.setBool(valueIndex,false);
}
void* UIFrameArray::Frame::getValue(uint32 index, uint32 typeSize, uint32 vpf) {
    //return (Value*)(memory.data+ vpf/8+index*sizeof(Value));
    return (void*)((char*)array.data + vpf + index * typeSize);
}
bool UIFrameArray::Frame::getBool(uint32 index) {
    //uint32 i = index / 8;
    //uint32 j = index % 8;
    //char byte = memory.data[i];
    //char bit = byte&(1<<j);

    return *((char*)array.data + index);
}
void UIFrameArray::Frame::setBool(uint32 index, bool yes) {
    //uint32 i = index / 8;
    //uint32 j = index % 8;
    //if (yes) {
    //	memory.data[i] = memory.data[i] | (1 << j);
    //} else {
    //	memory.data[i] = memory.data[i] & (~(1 << j));
    //}
    *((char*)array.data + index) = yes;
}
// bool UIFrameArray::copy(UIFrameArray& array){
//     for(int i=0;i<array.m_frames.count;i++){
//         ((Frame*)array.m_frames.data)->array.reserve(0);
//     }
//     array.m_frames.reserve(0);
    
//     array.m_valuesPerFrame = m_valuesPerFrame; 
//     array.m_typeSize = m_typeSize;
//     // if(array.m_frames.m_typeSize != m_frames.m_typeSize){
//     //     array.m_frames.reserve(0);
//     // }
//     array.m_frames.m_typeSize = m_frames.m_typeSize;
//     if(m_frames.max != array.m_frames.max){
//         array.m_frames.reserve(m_frames.max);
//     }
//     // array.m_frames.count = m_frames.count;
//     for(int i=0;i<array.m_frames.count;i++){
//         ((Frame*)array.m_frames.data)->array.
//     }
//     // 
// }
UIBox* UIModule::makeBox(uint64 id){
     if(id){
        int found=-1;
        for(int i=0;i<internalBoxes.count;i++){
            auto t = (UIInternal*)internalBoxes.data + i;
            if(t->id == id){
                found = i;
                break;   
            }
        }
        if(found==-1){
            UIBox* ptr = 0;
            uint32 index = maintainedBoxes.add(0,(void**)&ptr);
            if(index==-1) return 0;
            if(index>=internalBoxes.max){
                if(!internalBoxes.reserve(internalBoxes.max*2+100)){
                    maintainedBoxes.remove(index);
                    return 0;
                }
            }
            
            UIElement elem;
            elem.index = index;
            elem.type = UI_ELEMENT_BOX|UI_ELEMENT_MAINTAIN;
            internalElements.push_back(elem);
            
            UIInternal* internal = (UIInternal*)internalBoxes.data+index;
            internal->id = id;
            internalBoxes.count++;
            
             new(ptr)UIBox();
            
            return ptr;
        }else{
            // Todo: What if we already added element for this frame? do it again?
            UIElement elem;
            elem.index = found;
            elem.type = UI_ELEMENT_BOX|UI_ELEMENT_MAINTAIN;
            internalElements.push_back(elem);
            
            return (UIBox*)maintainedBoxes.get(found);
        }
    }else{
        if(boxes.count==boxes.max){
            if(!boxes.reserve(boxes.max*2+100))
                return 0;   
        }
        
        UIElement elem;
        elem.index = boxes.count;
        elem.type = UI_ELEMENT_BOX;
        internalElements.push_back(elem);
        
        auto ptr = (UIBox*)boxes.data + boxes.count;
        boxes.count++;
        
        new(ptr)UIBox();
        
        return ptr;
    }
}
UIText* UIModule::makeText(uint64 id){
    if(id){
        int found=-1;
        for(int i=0;i<internalTexts.count;i++){
            auto t = (UIInternal*)internalTexts.data + i;
            if(t->id == id){
                found = i;
                break;   
            }
        }
        if(found==-1){
            UIText* ptr = 0;
            uint32 index = maintainedTexts.add(0,(void**)&ptr);
            if(index==-1) return 0;
            if(index>=internalTexts.max){
                if(!internalTexts.reserve(internalTexts.max*2+100)){
                    maintainedTexts.remove(index);
                    return 0;
                }
            }
            UIElement elem;
            elem.index = index;
            elem.type = UI_ELEMENT_TEXT|UI_ELEMENT_MAINTAIN;
            internalElements.push_back(elem);
            
            UIInternal* internal = (UIInternal*)internalTexts.data+index;
            internal->id = id;
            internalTexts.count++;
            
            new(ptr)UIText();
            
            return ptr;
        }else{
            // Todo: What if we already added element for this frame? do it again?
            UIElement elem;
            elem.index = found;
            elem.type = UI_ELEMENT_TEXT|UI_ELEMENT_MAINTAIN;
            internalElements.push_back(elem);
            
            return (UIText*)maintainedTexts.get(found);
        }
    }else{
        if(texts.count==texts.max){
            if(!texts.reserve(texts.max*2+100))
                return 0;   
        }
        
        UIElement elem;
        elem.index = texts.count;
        elem.type = UI_ELEMENT_TEXT;
        internalElements.push_back(elem);
        
        auto ptr = (UIText*)texts.data + texts.count;
        texts.count++;
        
        new(ptr)UIText();
        
        return ptr;
    }
}
void UIModule::removeBox(uint64 id){
    int found=-1;
    for(int i=0;i<internalBoxes.count;i++){
        auto t = (UIInternal*)internalBoxes.data + i;
        if(t->id == id){
            found = i;
            break;   
        }
    }
    if(found!=-1){
        ((UIInternal*)internalBoxes.data)[found].id = 0;
        maintainedBoxes.remove(found);   
    }
}
void UIModule::removeText(uint64 id){
    int found=-1;
    for(int i=0;i<internalTexts.count;i++){
        auto t = (UIInternal*)internalTexts.data + i;
        if(t->id == id){
            found = i;
            break;   
        }
    }
    if(found!=-1){
        ((UIInternal*)internalTexts.data)[found].id = 0;
        maintainedTexts.remove(found);   
    }   
}
void UIModule::cleanup(){
    if(vertexData)
        delete[] vertexData;
    
    initialized=false;
    
    printf("UIModule : CLEANUP NOT IMPLEMENTED\n");
    
}
int UIModule::readTexture(const char* path){
    UITexture texture;
    
    int channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path,&texture.width,&texture.height,&channels,4); // 4 = rgba
    
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D,texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D,0);
    GL_CHECK_UI();
    textures.push_back(texture);
    return textures.size()-1;
}
int UIModule::readFont(const char* path){
    
    int id = readTexture(path);
    if(id==-1)
        return -1;
        
    UITexture& texture = textures[id];

    UIFont font;
    font.textureId = id;
    font.charWidth = 35;
    font.cellW = texture.width/16;
    font.cellH = texture.height/16;
    // if(texture.width!=texture.height) printf("UIModule : read font is not square!");
    
    fonts.push_back(font);
    return fonts.size()-1;
}
unsigned int UIModule::location(const std::string& name){
    auto find = shaderLocations.find(name);
    if(find==shaderLocations.end()){
        unsigned int loc = glGetUniformLocation(shaderId, name.c_str());
        shaderLocations[name] = loc;
        return loc;
    }else{
        return find->second;
    }
}
void UIModule::init(){
      if(!initialized){
        shaderId = CreateUIShader();
        
        glGenVertexArrays(1,&vertexArrayId);
        glGenBuffers(1,&vertexBufferId);
        glGenBuffers(1,&indexBufferId);
        
        unsigned int* indices = new unsigned int[6*BATCH_BOX_LIMIT];
        for (int i = 0; i < BATCH_BOX_LIMIT; i++) {
            indices[i * 6] = i * 4 + 0;
            indices[i * 6 + 1] = i * 4 + 1;
            indices[i * 6 + 2] = i * 4 + 2;
            indices[i * 6 + 3] = i * 4 + 2;
            indices[i * 6 + 4] = i * 4 + 3;
            indices[i * 6 + 5] = i * 4 + 0;
        }
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*BATCH_BOX_LIMIT*sizeof(unsigned int), indices, GL_STATIC_DRAW);
        
        delete[] indices;
        
        vertexData = new UIVertex[BATCH_BOX_LIMIT*4];
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
        glBufferData(GL_ARRAY_BUFFER, 4*BATCH_BOX_LIMIT*sizeof(UIVertex), 0, GL_DYNAMIC_DRAW);
        
        glBindVertexArray(vertexArrayId);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(0 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(8 * sizeof(float)));
        
        readFont("assets\\fonts\\consolas42.png");
        // readFont("D:\\Backup\\CodeProjects\\ProjectUnknown\\ProjectUnknown\\assets\\fonts\\consolas42.png");
        
        initialized = true;
    }
}
void UIModule::drawBatch(int& dataIndex){
    if(dataIndex!=BATCH_BOX_LIMIT){
        memset(vertexData+4*dataIndex,0, (BATCH_BOX_LIMIT-dataIndex)*sizeof(UIVertex)*4);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER,vertexBufferId);
    glBufferSubData(GL_ARRAY_BUFFER,0,BATCH_BOX_LIMIT*4*sizeof(UIVertex), vertexData);
    
    glBindVertexArray(vertexArrayId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId);
    glDrawElements(GL_TRIANGLES, 6*BATCH_BOX_LIMIT, GL_UNSIGNED_INT, nullptr);
    drawcalls++;
    // printf("UIModule : Draw string batch %d\n",index);
    
    dataIndex=0;   
    
}
void UIModule::render(float windowWidth, float windowHeight){
    if(!initialized){
        init();
    }
    if(!shaderId||!vertexArrayId||!vertexBufferId||!indexBufferId) return;
    
    glUseProgram(shaderId);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    
    // printf("ui %f %f\n", windowWidth, windowHeight);
    glUniform2f(location("uWindow"),windowWidth,windowHeight);

    char strbuf[20];
    for(int i=0;i<SHADER_SAMPLER_LIMIT;i++){
        sprintf(strbuf,"uSampler[%d]",i);
        glUniform1i(location(strbuf),i);
    }
    
    // Swap arrays
    std::swap(tempElements,internalElements);
    internalElements.clear();
    
    UIArray tmp = tempBoxes;
    tempBoxes = boxes;
    boxes = tmp;
    boxes.count = 0;
    
    tmp = tempTexts;
    tempTexts = texts;
    texts = tmp;
    texts.count = 0;
    
    if(tempMaintainedBoxes.max<internalBoxes.count)
        tempMaintainedBoxes.reserve(internalBoxes.count);
    for(int i=0;i<internalBoxes.count;i++){
        *((UIBox*)tempMaintainedBoxes.data+i) = *(UIBox*)maintainedBoxes.get(i);
    }
    
    if(tempMaintainedTexts.max<internalTexts.count)
        tempMaintainedTexts.reserve(internalTexts.count);
    for(int i=0;i<internalTexts.count;i++){
        new((UIText*)tempMaintainedTexts.data+i)UIText();
        *((UIText*)tempMaintainedTexts.data+i) = *(UIText*)maintainedTexts.get(i);
    }

    int index=0; // total index
    int dataIndex=0; // index in vertex data
    std::unordered_map<int,float> textureMapping; // Use array instead of map?
    while(true){
        if(dataIndex==BATCH_BOX_LIMIT){
            drawBatch(dataIndex);
        }
        if(index == tempElements.size()){
            drawBatch(dataIndex);
            break;
        }
        
        UIElement& elem = tempElements[index];
        index++;
        
        int rawType = elem.type&(~UI_ELEMENT_MAINTAIN);
        if(rawType == UI_ELEMENT_BOX){
            
            UIBox* e = 0;
            if(elem.type&UI_ELEMENT_MAINTAIN){
                e = (UIBox*)tempMaintainedBoxes.data + elem.index;
                // e = (UIBox*)maintainedBoxes.get(elem.index);
            }else{
                e = (UIBox*)tempBoxes.data+elem.index;
            }
            
            float sampler = -1;
            if(e->textureId!=-1){
                if(textures.size() <= e->textureId) continue;
                
                auto find = textureMapping.find(e->textureId);
                if(find!=textureMapping.end()){
                    sampler = find->second;
                }else{
                    if(textureMapping.size()==SHADER_SAMPLER_LIMIT){
                        // sampler is full, render batch
                        drawBatch(dataIndex);
                        
                        textureMapping.clear();
                    }
                    
                    glActiveTexture(GL_TEXTURE0+textureMapping.size());
                    glBindTexture(GL_TEXTURE_2D,textures[e->textureId].id);
                    sampler = textureMapping.size();
                    textureMapping[e->textureId] = textureMapping.size();
                }
            }
            
            vertexData[dataIndex * 4 + 0] = { e->x, e->y, 0, 0, e->color.r, e->color.g, e->color.b, e->color.a, sampler };
            vertexData[dataIndex * 4 + 1] = { e->x, e->y + e->h, 0, 1, e->color.r, e->color.g, e->color.b, e->color.a, sampler };
            vertexData[dataIndex * 4 + 2] = { e->x + e->w, e->y + e->h, 1, 1, e->color.r, e->color.g, e->color.b, e->color.a, sampler };
            vertexData[dataIndex * 4 + 3] = { e->x + e->w, e->y, 1, 0, e->color.r, e->color.g, e->color.b, e->color.a, sampler };
            dataIndex++;
        }
        if(rawType == UI_ELEMENT_TEXT){
            UIText* t=0;
            if(elem.type&UI_ELEMENT_MAINTAIN){
                // t = (UIText*)maintainedTexts.get(elem.index);
                t = (UIText*)tempMaintainedTexts.data+elem.index;
            }else{
                t = (UIText*)tempTexts.data+elem.index;
            }
                        
            int length = t->getLength();
            // int length = strlen(t->text);
            if(fonts.size()<=t->fontId) continue;
            UIFont& font = fonts[t->fontId];
            if(textures.size()<=font.textureId) continue;
            UITexture& texture = textures[font.textureId];
            
            float sampler = -1;
            if(font.textureId!=-1){
                auto find = textureMapping.find(font.textureId);
                if(find!=textureMapping.end()){
                    sampler = find->second;
                }else{
                    if(textureMapping.size()==SHADER_SAMPLER_LIMIT){
                        // sampler is full, render batch
                        drawBatch(dataIndex);
                        
                        textureMapping.clear();
                    }
                    
                    // printf("UIModule : Bind texture %d\n",font.textureId);
                    glActiveTexture(GL_TEXTURE0+textureMapping.size());
                    glBindTexture(GL_TEXTURE_2D, texture.id);
                    sampler = textureMapping.size();
                    textureMapping[font.textureId] = textureMapping.size();
                }
            }
            
            float uw = (float)font.charWidth/texture.width;
            float vh = (float)font.cellH/texture.height;
            float w = (float)font.charWidth/font.cellW * t->h;
            float h = t->h;
            
            float cw = w*0.1;
            float ch = h;
            int cursorIndex = t->cursorIndex;
            if(cursorIndex<0) cursorIndex=0;
            if(cursorIndex>length) cursorIndex=length;
            
            float x = t->x;
            float y = t->y;
            int chrIndex = 0;
            while(true){
                if(t->editing){
                    if(cursorIndex==chrIndex){
                        vertexData[dataIndex * 4 + 0] = { x, y, 0, 0, t->color.r, t->color.g, t->color.b, t->color.a, -1 };
                        vertexData[dataIndex * 4 + 1] = { x, y + ch, 0, 1, t->color.r, t->color.g, t->color.b, t->color.a, -1 };
                        vertexData[dataIndex * 4 + 2] = { x + cw, y + ch, 1, 1, t->color.r, t->color.g, t->color.b, t->color.a,-1 };
                        vertexData[dataIndex * 4 + 3] = { x + cw, y, 1, 0, t->color.r, t->color.g, t->color.b, t->color.a, -1 };
                        dataIndex++;
                        
                         if(dataIndex==BATCH_BOX_LIMIT){
                            drawBatch(dataIndex);
                        }
                    }   
                }
                if(chrIndex==length)
                    break;
                Assert(t->getStr());
                unsigned char chr = t->getStr()[chrIndex];
                chrIndex++;
                
                if(chr=='\n'){
                    x = t->x;
                    y += h;
                    continue;
                }
                
                float u = (float)font.cellW*(chr%16)/texture.width;
                float v = (float)font.cellH*(chr/16)/texture.height;
                
                vertexData[dataIndex * 4 + 0] = { x, y, u, v, t->color.r, t->color.g, t->color.b, t->color.a, sampler };
                vertexData[dataIndex * 4 + 1] = { x, y + h, u, v+vh, t->color.r, t->color.g, t->color.b, t->color.a, sampler };
                vertexData[dataIndex * 4 + 2] = { x + w, y + h, u+uw, v+vh, t->color.r, t->color.g, t->color.b, t->color.a, sampler };
                vertexData[dataIndex * 4 + 3] = { x + w, y, u+uw, v, t->color.r, t->color.g, t->color.b, t->color.a, sampler };
                dataIndex++;
                
                x += w;
                
                if(dataIndex==BATCH_BOX_LIMIT){
                    drawBatch(dataIndex);
                }
            }
        }
    }
    // texts.count = 0;
    // boxes.count = 0;
    // internalElements.clear();
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    
    frameCount++;
    if(frameCount>=60){
        // printf("%.1f drawcalls per frame\n",(float)drawcalls/frameCount);
        drawcalls=0;
        frameCount=0;
    }
}
void TestUIBoxes(UIModule& ui, float winw){
    int total=100000;
    int sqr = sqrt(total);
    int bw = 5;
    int mw = winw/bw-4.f;
    for(int i=0;i<total;i++){
        auto a = ui.makeBox();
        if(a)
            *a = {10.f+bw*(i%mw),60.f+bw*(i/mw),bw-1.f,bw-1.f,{(i%sqr)/(float)sqr,(float)i/(total),1-(i/sqr)/(float)sqr,1}};
        // *a = {i*50.f,500+i*i*10.f-i*100,40,40,{i*i*0.01f,1-i/10.f,1-i*0.1f,1}};
    }   
}
void TestUITexts(UIModule& ui, float winw){
    int total=8000;
    int sqr = sqrt(total);
    float bh = 15;
    float bw = 6*bh*35/64.f;
    int mw = winw/bw-2;
    if(mw==0) mw = 1;
    const char* texts[]{"Hello","Sup","Bro","meso","Soup"};
    for(int i=0;i<total;i++){
        auto a = ui.makeText();
        int ind = (2*i+1)%(sizeof(texts)/sizeof(const char*));
        if(a)
            *a = {10.f+bw*(i%mw),
            60.f+bh*(i/mw),
            bh,
            (uint32)strlen(texts[ind]),
            (char*)texts[ind],
            {(i%sqr)/(float)sqr,(float)i/(total),1-(i/sqr)/(float)sqr,1}};
        
    }   
}
void TestUIMaintain(UIModule& ui){
    // auto& a = *ui.makeText("a");
    // auto& b = *ui.makeText("a");
    // // auto& b = *ui.makeBox();
    // // b = {300,300,50,50, {1,0,1,1}};
    // if(a.x==0){
    //     a = {300,300,50,"Hello", {1,0,1,1}};
    //     b = {300,400,50,"Hellows", {1,0,1,1}};
    //     b.editing = true;
    // }
    
    // ui.edit(&b);
    
    for(int i=0;i<32;i++){
        auto& p = *ui.makeBox(i);
        // p = {i*10.f+10,200,8,8,{i/32.f,1,1,1}};
        if(p.x==0)
            p = {i*10.f+10,200,8,8,{i/32.f,1-i/32.f,i*i/(32.f*32.f)-2*i/32.f,1}};
        else{
            p.x += 0.2*(i%2)*(32-i)/32.f;
            p.y += 0.2*((1+i-16)%2)*(32-i)/32.f;   
        }
    }
    for(int i=0;i<32;i++){
        auto& p = *ui.makeText(i);
        // p = {i*10.f+10,200,8,8,{i/32.f,1,1,1}};
        if(p.x==0)
            p = {i*10.f+10,200,15,3,"Sup",{i/32.f,1-i/32.f,i*i/(32.f*32.f)-2*i/32.f,1}};
            // p = {i*10.f+10,200,8,8,{i/32.f,1-i/32.f,i*i/(32.f*32.f)-2*i/32.f,1}};
        else{
            p.x += 0.5*(i%2)*(32-i)/32.f;
            p.y += 0.5*((1+i-16)%2)*(32-i)/32.f;   
        }
    }
}
}