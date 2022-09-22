#pragma once

class Text {
    std::string buffer = {
        "abcdefghijklmnopqrstuvwxyz\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n" };

public:
    void process(WPARAM key) {
        // [TODO] block cursor.
        // [TODO] space+Q to quit.
        // [TODO] h/j/k/l to move.
        // [TODO] insert/normal mode.
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
            // [TODO]
            buffer += (char)128/*custom BLOCK*/;
        }
        else if (key == 127/*DEL*/) {
            // [TODO]
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

