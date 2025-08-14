#include "oz/editor/editor.h"
#include "oz/oz_log.h"
#include <stdlib.h>

int main(int argc, char** argv) {
    OZ_INFO("Starting OzWorld Editor (Modular Version)");
    
    // Create editor instance
    OzEditor* editor = oz_editor_create();
    if (!editor) {
        OZ_ERROR("Failed to create editor instance");
        return 1;
    }
    
    // Initialize editor
    if (!oz_editor_initialize(editor, argc, argv)) {
        OZ_ERROR("Failed to initialize editor");
        oz_editor_destroy(editor);
        return 1;
    }
    
    // Run main loop
    int result = oz_editor_run(editor);
    
    // Cleanup
    oz_editor_destroy(editor);
    
    OZ_INFO("OzWorld Editor exited with code: %d", result);
    return result;
}
