
struct FontGlyph {
	uint16_t id = 0;
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float x_off = 0.0f;
	float y_off = 0.0f;
	float x_adv = 0.0f;

	FontGlyph(uint16_t id, float x, float y, float w, float h, float x_off, float y_off, float x_adv)
		: id(id), x(x), y(y), w(w), h(h), x_off(x_off), y_off(y_off), x_adv(x_adv) {}
};

