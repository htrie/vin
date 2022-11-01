#pragma once

struct Colors {
	Color clear = Color::rgba(1, 22, 39, 255);
	Color overlay = Color::rgba(31, 52, 69, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color cursor_line = Color::rgba(65, 80, 29, 255);
	Color whitespace = Color::rgba(11, 32, 42, 255);
	Color text = Color::rgba(200, 200, 200, 255);
	Color text_cursor = Color::rgba(5, 5, 5, 255);
	Color line_number = Color::rgba(75, 100, 121, 255);
	Color highlight = Color::rgba(255, 0, 255, 255);
	Color diff_note = Color::rgba(255, 192, 0, 255);
	Color diff_add = Color::rgba(0, 192, 0, 255);
	Color diff_remove = Color::rgba(192, 0, 0, 255);
	Color keyword = Color::rgba(190, 190, 190, 255);
	Color clas = Color::rgba(180, 180, 180, 255);
	Color comment = Color::rgba(255, 255, 0, 255);
	Color punctuation = Color::rgba(127, 219, 202, 255);
	Color quote = Color::rgba(247, 140, 108, 255);
};

const Colors colors() {
	static Colors colors;
	return colors;
}

