#pragma once

struct Colors {
#if 1
	Color clear = Color::rgba(1, 22, 39, 255);
	Color overlay = Color::rgba(31, 52, 69, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color cursor_line = Color::rgba(65, 80, 29, 255);
	Color whitespace = Color::rgba(11, 32, 42, 255);
	Color text = Color::rgba(200, 200, 200, 255);
	Color text_cursor = Color::rgba(5, 5, 5, 255);
	Color line_number = Color::rgba(75, 100, 121, 255);
	Color highlight = Color::rgba(192, 0, 192, 255);
	Color diff_note = Color::rgba(255, 192, 0, 255);
	Color diff_add = Color::rgba(0, 192, 0, 255);
	Color diff_remove = Color::rgba(192, 0, 0, 255);
	Color keyword = Color::rgba(199, 146, 234, 255);
	Color clas = Color::rgba(180, 180, 180, 255);
	Color comment = Color::rgba(255, 255, 0, 255);
	Color punctuation = Color::rgba(127, 219, 202, 255);
	Color quote = Color::rgba(247, 140, 108, 255);
#else
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
	Color quote = Color::gray(120);
#endif
};

const Colors colors() {
	static Colors colors;
	return colors;
}


struct HSB {
	float hue_start = 0.0f;
	float hue_range = 260.0f;
	float hue_adjust = 10.0f;
	float saturation = 0.50f;
	float brightness = 0.85f;

	std::string increase_hue_start() { hue_start = std::clamp(hue_start + 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }
	std::string decrease_hue_start() { hue_start = std::clamp(hue_start - 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }

	std::string increase_hue_range() { hue_range = std::clamp(hue_range + 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }
	std::string decrease_hue_range() { hue_range = std::clamp(hue_range - 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }

	std::string increase_hue_adjust() { hue_adjust = std::clamp(hue_adjust + 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }
	std::string decrease_hue_adjust() { hue_adjust = std::clamp(hue_adjust - 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }

	std::string increase_saturation() { saturation = std::clamp(saturation + 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }
	std::string decrease_saturation() { saturation = std::clamp(saturation - 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }

	std::string increase_brightness() { brightness = std::clamp(brightness + 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }
	std::string decrease_brightness() { brightness = std::clamp(brightness - 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }
};

HSB& hsb() {
	static HSB hsb;
	return hsb;
}


struct Spacing {
	float character = 7.0f;
	float line = 15.0f;

	std::string increase_char_width() { character = std::clamp(character + 0.05f, 0.0f, 10.0f); return std::string("char_width = ") + std::to_string(character); }
	std::string decrease_char_width() { character = std::clamp(character - 0.05f, 0.0f, 10.0f); return std::string("char_width = ") + std::to_string(character); }

	std::string increase_char_height() { line = std::clamp(line + 0.05f, 0.0f, 20.0f); return std::string("char_height = ") + std::to_string(line); }
	std::string decrease_char_height() { line = std::clamp(line - 0.05f, 0.0f, 20.0f); return std::string("char_height = ") + std::to_string(line); }
};

Spacing& spacing() {
	static Spacing spacing;
	return spacing;
}

