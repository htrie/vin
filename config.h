#pragma once

struct Colors {
	Color clear;
	Color overlay;
	Color cursor;
	Color cursor_line;
	Color whitespace;
	Color text;
	Color text_cursor;
	Color long_line;
	Color line_number;
	Color highlight;
	Color diff_note;
	Color diff_add;
	Color diff_remove;
	Color keyword;
	Color clas;
	Color comment;
	Color punctuation;
	Color quote;

	void use_rgb() {
		clear = Color::rgba(32, 32, 18, 255);
		overlay = Color::rgba(31, 52, 69, 255);
		cursor = Color::rgba(255, 255, 0, 255);
		cursor_line = Color::rgba(65, 80, 29, 255);
		whitespace = Color::rgba(11, 32, 42, 255);
		text = Color::rgba(200, 200, 200, 255);
		text_cursor = Color::rgba(5, 5, 5, 255);
		long_line = Color::rgba(255, 0, 0, 255);
		line_number = Color::rgba(75, 100, 121, 255);
		highlight = Color::rgba(192, 0, 192, 255);
		diff_note = Color::rgba(255, 192, 0, 255);
		diff_add = Color::rgba(0, 192, 0, 255);
		diff_remove = Color::rgba(192, 0, 0, 255);
		keyword = Color::rgba(199, 146, 234, 255);
		clas = Color::rgba(180, 180, 180, 255);
		comment = Color::rgba(255, 255, 0, 255);
		punctuation = Color::rgba(127, 219, 202, 255);
		quote = Color::rgba(247, 140, 108, 255);
	}

	void use_gray() {
		clear = Color::gray(247);
		overlay = Color::gray(237);
		cursor = Color::gray(0);
		cursor_line = Color::gray(217);
		whitespace = Color::gray(240);
		text = Color::gray(5);
		text_cursor = Color::gray(220);
		long_line = Color::gray(80);
		line_number = Color::gray(140);
		highlight = Color::gray(188);
		diff_note = Color::gray(108);
		diff_add = Color::gray(0);
		diff_remove = Color::gray(166);
		keyword = Color::gray(100);
		clas = Color::gray(50);
		comment = Color::gray(166);
		punctuation = Color::gray(158);
		Color quote = Color::gray(120);
	}
};

Colors& colors() {
	static Colors colors;
	return colors;
}


struct HSB {
	float hue_start = 0.0f;
	float hue_range = 260.0f;
	float hue_adjust = 10.0f;
	float saturation = 0.50f;
	float brightness = 0.85f;

	std::string increase_hue_start() { hue_start = clamp(hue_start + 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }
	std::string decrease_hue_start() { hue_start = clamp(hue_start - 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }

	std::string increase_hue_range() { hue_range = clamp(hue_range + 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }
	std::string decrease_hue_range() { hue_range = clamp(hue_range - 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }

	std::string increase_hue_adjust() { hue_adjust = clamp(hue_adjust + 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }
	std::string decrease_hue_adjust() { hue_adjust = clamp(hue_adjust - 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }

	std::string increase_saturation() { saturation = clamp(saturation + 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }
	std::string decrease_saturation() { saturation = clamp(saturation - 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }

	std::string increase_brightness() { brightness = clamp(brightness + 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }
	std::string decrease_brightness() { brightness = clamp(brightness - 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }

	void use_rgb() {
		hue_start = 0.0f;
		hue_range = 260.0f;
		hue_adjust = 10.0f;
		saturation = 0.50f;
		brightness = 0.85f;
	}

	void use_gray() {
		hue_start = 0.0f;
		hue_range = 0.0f;
		hue_adjust = 0.0f;
		saturation = 1.00f;
		brightness = 0.00f;
	}
};

HSB& hsb() {
	static HSB hsb;
	return hsb;
}


struct Spacing {
	float character = 7.0f;
	float line = 15.0f;

	std::string increase_char_width() { character = clamp(character + 0.05f, 0.0f, 10.0f); return std::string("char_width = ") + std::to_string(character); }
	std::string decrease_char_width() { character = clamp(character - 0.05f, 0.0f, 10.0f); return std::string("char_width = ") + std::to_string(character); }

	std::string increase_char_height() { line = clamp(line + 0.05f, 0.0f, 20.0f); return std::string("char_height = ") + std::to_string(line); }
	std::string decrease_char_height() { line = clamp(line - 0.05f, 0.0f, 20.0f); return std::string("char_height = ") + std::to_string(line); }
};

Spacing& spacing() {
	static Spacing spacing;
	return spacing;
}

