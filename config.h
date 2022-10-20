#pragma once

struct Colors {
	Color clear = Color::rgba(247, 247, 247, 255);
	Color overlay = Color::rgba(227, 227, 227, 255);
	Color cursor = Color::rgba(0, 0, 0, 255);
	Color cursor_line = Color::rgba(166, 166, 166, 255);
	Color whitespace = Color::rgba(220, 220, 220, 255);
	Color text = Color::rgba(50, 50, 50, 255);
	Color text_cursor = Color::rgba(220, 220, 220, 255);
	Color line_number = Color::rgba(180, 180, 180, 255);
	Color highlight = Color::rgba(148, 148, 148, 255);
	Color diff_note = Color::rgba(128, 128, 128, 255);
	Color diff_add = Color::rgba(0, 0, 0, 255);
	Color diff_remove = Color::rgba(200, 200, 200, 255);
	Color cpp_comment = Color::rgba(200, 200, 200, 255);
	Color cpp_keyword = Color::rgba(0, 0, 0, 255);
	Color cpp_punctuation = Color::rgba(128, 128, 128, 255);
	Color cpp_number = Color::rgba(100, 100, 100, 255);
	Color cpp_string = Color::rgba(80, 80, 80, 255);
};

const Colors colors() {
	static Colors colors;
	return colors;
}

