#pragma once

struct Colors {
	Color clear = Color::rgba(1, 22, 39, 255);
	Color overlay = Color::rgba(31, 52, 69, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color cursor_line = Color::rgba(65, 80, 29, 255);
	Color whitespace = Color::rgba(15, 40, 33, 255);
	Color text = Color::rgba(205, 226, 239, 255);
	Color text_cursor = Color::rgba(5, 5, 5, 255);
	Color line_number = Color::rgba(75, 100, 121, 255);
	Color highlight = Color::rgba(255, 0, 255, 255);
	Color diff_note = Color::rgba(255, 192, 0, 255);
	Color diff_add = Color::rgba(0, 192, 0, 255);
	Color diff_remove = Color::rgba(192, 0, 0, 255);
	Color cpp_comment = Color::rgba(255, 255, 0, 255);
	Color cpp_punctuation = Color::rgba(127, 219, 202, 255);
	Color cpp_quote = Color::rgba(247, 140, 108, 255);
	Color cpp_string = Color::rgba(247, 140, 108, 255);
};

const Colors colors() {
	static Colors colors;
	return colors;
}

