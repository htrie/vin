#pragma once

struct Colors {
	Color clear = Color::gray(247);
	Color overlay = Color::gray(237);
	Color cursor = Color::gray(0);
	Color cursor_line = Color::gray(217);
	Color whitespace = Color::gray(240);
	Color text = Color::gray(5);
	Color text_cursor = Color::gray(220);
	Color line_number = Color::gray(140);
	Color highlight = Color::gray(188);
	Color diff_note = Color::gray(108);
	Color diff_add = Color::gray(0);
	Color diff_remove = Color::gray(166);
	Color keyword = Color::gray(100);
	Color clas = Color::gray(50);
	Color comment = Color::gray(166);
	Color punctuation = Color::gray(158);
	Color quote = Color::gray(178);
};

const Colors colors() {
	static Colors colors;
	return colors;
}

