#pragma once

class Text {
    std::string buffer = {
        "abcdefghijklmnopqrstuvwxyz\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n" };

public:
    void process(WPARAM key) {
        // [TODO] Display block cursor.
        // [TODO] Handle space+Q to quit.
        if (key == 8/*BS*/) {
            if (!buffer.empty())
                buffer.pop_back();
        }
        else if (key == 9/*TAB*/) {
            buffer += "    ";
        }
        else if (key == 13/*CR*/) {
            buffer += '\n';
        }
        else if (key == 27/*ESC*/) {
            // [TODO] Handle escape.
        }
        else if (key == 127/*DEL*/) {
            // [TODO] Handle delete.
        }
        else {
            buffer += (char)key;
        }
    }

    std::string cull() {
        // [TODO] Clip lines to fit screen.
        return buffer;
    }
};

