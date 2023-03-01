#pragma once

struct Colors {
	Color clear = Color::gray(30);
	Color column_indicator = Color::gray(28);
	Color overlay = Color::gray(60);
	Color cursor_line = Color::rgba(80, 80, 0, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color highlight = Color::rgba(180, 0, 180, 255);
	Color whitespace = Color::gray(30);
	Color bar = Color::rgba(59, 56, 54, 255);
	Color bar_text = Color::rgba(120, 110, 100, 255);
	Color bar_pattern = Color::rgba(180, 165, 150, 255);
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


struct Spacing {
	float character = 8.0f;
	float line = 20.0f;
	float zoom = 1.0f;
	unsigned tab = 3;

	std::string increase_char_width() { character = std::clamp(character + 0.05f, 0.0f, 20.0f); return std::string("char_width = ") + std::to_string(character); }
	std::string decrease_char_width() { character = std::clamp(character - 0.05f, 0.0f, 20.0f); return std::string("char_width = ") + std::to_string(character); }

	std::string increase_char_height() { line = std::clamp(line + 0.05f, 0.0f, 40.0f); return std::string("char_height = ") + std::to_string(line); }
	std::string decrease_char_height() { line = std::clamp(line - 0.05f, 0.0f, 40.0f); return std::string("char_height = ") + std::to_string(line); }

	std::string increase_char_zoom() { zoom = std::clamp(zoom + 0.1f, 0.5f, 2.0f); return std::string("char_zoom = ") + std::to_string(zoom); }
	std::string decrease_char_zoom() { zoom = std::clamp(zoom - 0.1f, 0.5f, 2.0f); return std::string("char_zoom = ") + std::to_string(zoom); }

	std::string increase_tab_size() { tab = std::clamp(tab + 1, 0u, 8u); return std::string("tab_size = ") + std::to_string(tab); }
	std::string decrease_tab_size() { tab = std::clamp(tab > 0 ? tab - 1 : 0u, 0u, 8u); return std::string("tab_size = ") + std::to_string(tab); }
};

Spacing& spacing() {
	static Spacing spacing;
	return spacing;
}

