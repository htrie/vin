#pragma once

struct Colors {
	Color clear = Color::gray(30);
	Color column_indicator = Color::gray(28);
	Color cursor_line = Color::rgba(80, 80, 0, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color highlight = Color::rgba(180, 0, 180, 255);
	Color whitespace = Color::gray(30);
	Color bar = Color::rgba(59, 56, 54, 255);
	Color bar_text = Color::rgba(120, 110, 100, 255);
	Color text = Color::rgba(229, 218, 184, 255);
	Color text_cursor = Color::gray(30);
	Color keyword = Color::rgba(199, 146, 234, 255);
	Color clas = Color::rgba(255, 203, 139, 255);
	Color function = Color::rgba(130, 170, 255, 255);
	Color punctuation = Color::rgba(127, 219, 202, 255);
	Color number = Color::rgba(247, 140, 84, 255);
	Color line_number = Color::gray(80);
	Color context = Color::gray(140);
	Color quote = Color::rgba(255, 0, 128, 255);
	Color comment = Color::rgba(255, 255, 0, 255);
	Color diff_note = Color::rgba(255, 200, 0, 255);
	Color diff_add = Color::rgba(0, 200, 0, 255);
	Color diff_remove = Color::rgba(200, 0, 0, 255);
};

Colors& colors() {
	static Colors colors;
	return colors;
}
