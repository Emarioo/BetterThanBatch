#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>
#include "Engone/Typedefs.h"

// It is here to avoid including glfw3.h
#ifndef GLFW_MOUSE_BUTTON_1
#define GLFW_MOUSE_BUTTON_1 0
#endif

#define DISABLE_USER_INPUT

// don't mind these (;
typedef uint32_t uint32;
typedef uint64_t u64;

namespace engone {
    struct UIColor{
        float r,g,b,a;  
    };
    #define UI_ELEMENT_MAINTAIN 1
    #define UI_ELEMENT_BOX 0
    #define UI_ELEMENT_TEXT 2
    // struct UIElement {
    //     int type; // 0 box, 2 text, 1 keep (for box and text)
    //     int index;
    // };
    struct UIBox {
        float x, y, w, h;
        UIColor color;
        int textureId=-1;
    };
    struct UIText {
        float x, y, h;
        u32 length = 0;
        char* string = 0;
        UIColor color;
        int fontId=0;
        
        bool editing=false;
        int cursorIndex=-1;

        u32 getLength() const { return length; }
        char* getStr() const { return string; }
    };
    struct UITexture{
        unsigned int id;
        int width,height;
    };
    struct UIFont{
        int textureId;
        int charWidth;
        int cellW;
        int cellH;
    };
    struct UIVertex{
        float x, y, v, h, r, g, b, a, ti;  
    };
    struct UIArray {
        UIArray(int typeSize) : m_typeSize(typeSize) {}
        
        void* data=0;
        int count=0;
        int max=0;
        int m_typeSize;
        
        // bool add(void* ptr);
        bool reserve(int size);
    };
    // This class is a copy of FrameArray.h. Since UIModule doesn't rely on other
    // Files you can trivially copy UIModule header and source file into another project.
    class UIFrameArray {
    public:
        // constructor does nothing except remember the variable
        // valuesPerFrame is forced to be divisible by 8. (data alignment * bits as bools)
        UIFrameArray(uint32 typeSize, uint32 valuesPerFrame);
        ~UIFrameArray() { cleanup(); }
        // Does not call free on the items.
        void cleanup();
        
        // Returns -1 if something failed
        // Does not initialize the item
        // @param value pointer to an item which should be added, note that a memcpy occurs. value can be nullptr.
        // @param outPtr sets the pointer for the added object
        uint32 add(void* value, void** outPtr = nullptr);
        
        void* get(uint32 index);
        void remove(uint32 index);
        
        // array needs to be initialized. array = {} is enough.
        // bool copy(UIFrameArray& array);
        
    private:
        struct Frame {
            UIArray array{1};
            int count=0;
            void* getValue(uint32 index, uint32 typeSize, uint32 vpf);
            bool getBool(uint32 index);
            void setBool(uint32 index, bool yes);
        };

        UIArray m_frames{sizeof(Frame)};
        
        uint32 m_typeSize = 0;
        uint32 m_valuesPerFrame = 0;
    };

    // NOT THREAD SAFE
    // TEXTURE SAMPLING WITH BATCHING NOT TESTED
    class UIModule {
    public:
        UIModule() = default;
        ~UIModule() {cleanup();}
        void cleanup();
        void init(); // done in render function automatically
    
        // returns a number to identify the font
        int readFont(const char* path);
        // returns a number to identify the texture
        int readTexture(const char* path);
    
        // id as 0 will create a temporary element for the current frame.
        // Elements with non-zero ids will be maintained across frames untill removed. 
        
        UIBox* makeBox(u64 id=0);
        UIText* makeText(u64 id=0);
        void removeBox(u64 id);
        void removeText(u64 id);
        
        bool inside(UIBox* box, float x, float y);
        bool inside(UIText* box, float x, float y);
        
        bool clicked(UIBox* box, int mouseButton = GLFW_MOUSE_BUTTON_1);
        bool clicked(UIText* text, int mouseButton = GLFW_MOUSE_BUTTON_1);
        bool hover(UIBox* box);
        bool hover(UIText* text);
        void edit(UIText* text, bool stopEditWithEnter = true);
        
        // Calculates width of UIText based on font, height and string/text length
        float getWidthOfText(UIText* text);
        
        void render(float windowWidth, float windowHeight);
        
    private:
        struct UIElement {
            int type; // 0 box, 2 text, 1 keep (for box and text)
            int index;
        };
        struct UIInternal{
            u64 id;
        };
        // switch to C arrays
        std::vector<UIElement> tempElements;
        std::vector<UIElement> internalElements;
        std::vector<UIFont> fonts;
        std::vector<UITexture> textures;
        
        UIArray tempBoxes{sizeof(UIBox)};
        UIArray tempMaintainedBoxes{sizeof(UIBox)};
        UIArray boxes{sizeof(UIBox)};
        UIFrameArray maintainedBoxes{sizeof(UIBox),128};
        UIArray internalBoxes{sizeof(UIInternal)};
        
        UIArray tempTexts{sizeof(UIText)};
        UIArray tempMaintainedTexts{sizeof(UIText)};
        UIArray texts{sizeof(UIText)};
        UIFrameArray maintainedTexts{sizeof(UIText),128};
        UIArray internalTexts{sizeof(UIInternal)};    
        
        static const int BATCH_BOX_LIMIT=5000;
        static const int SHADER_SAMPLER_LIMIT=8;
        
        UIVertex* vertexData=0;
        int drawcalls=0;
        int frameCount=0;
        bool initialized=false;
        unsigned int shaderId=0;
        unsigned int vertexArrayId=0;
        unsigned int vertexBufferId=0;
        unsigned int indexBufferId=0;
        void drawBatch(int& dataIndex);
        
        unsigned int location(const std::string& name);
        std::unordered_map<std::string,unsigned int> shaderLocations;
    };

    void TestUIMaintain(UIModule& ui);
}
