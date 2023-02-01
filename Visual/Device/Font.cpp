
namespace Device
{

	std::unique_ptr<Font> Font::Create(IDevice* device)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		return std::make_unique<Font>(device);
	}

	static float Spacing(const char c)
	{
		switch (c)
		{
		case ',':
		case '.':
		case ';':
		case ':':
		case 'i':
			return 3.0f;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'j':
		case 'k':
		case 'l':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
			return 5.0f;
		default:
			return 6.0f;
		}
	}

	Font::Font(IDevice* device)
		: line(Line::Create(device))
	{
	}

	Font::~Font()
	{
	}

	void Font::Line2(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b)
	{
		vertex_count = 0;
		vertices[vertex_count++] = a;
		vertices[vertex_count++] = b;
		line->Draw(vertices.data(), vertex_count, color);
	}

	void Font::Line3(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c)
	{
		vertex_count = 0;
		vertices[vertex_count++] = a;
		vertices[vertex_count++] = b;
		vertices[vertex_count++] = c;
		line->Draw(vertices.data(), vertex_count, color);
	}

	void Font::Line4(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d)
	{
		vertex_count = 0;
		vertices[vertex_count++] = a;
		vertices[vertex_count++] = b;
		vertices[vertex_count++] = c;
		vertices[vertex_count++] = d;
		line->Draw(vertices.data(), vertex_count, color);
	}

	void Font::Line5(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e)
	{
		vertex_count = 0;
		vertices[vertex_count++] = a;
		vertices[vertex_count++] = b;
		vertices[vertex_count++] = c;
		vertices[vertex_count++] = d;
		vertices[vertex_count++] = e;
		line->Draw(vertices.data(), vertex_count, color);
	}

	void Font::Line6(std::unique_ptr<Line>& line, std::array<simd::vector2, 16>& vertices, unsigned& vertex_count, const simd::color& color, const simd::vector2& a, const simd::vector2& b, const simd::vector2& c, const simd::vector2& d, const simd::vector2& e, const simd::vector2& f)
	{
		vertex_count = 0;
		vertices[vertex_count++] = a;
		vertices[vertex_count++] = b;
		vertices[vertex_count++] = c;
		vertices[vertex_count++] = d;
		vertices[vertex_count++] = e;
		vertices[vertex_count++] = f;
		line->Draw(vertices.data(), vertex_count, color);
	}

	void Font::DrawText(int size, const std::string& text, Rect& rect, DWORD format, const simd::color& color, float user_scale)
	{
		line->Begin();

		const unsigned length = (unsigned)text.length();
		const float scale = user_scale * size / 10.0f;

		float full_width = 0.0f;
		for (unsigned i = 0; i < length; ++i)
		{
			full_width += Spacing(text[i]) * scale;
		}

		const float width = 4.0f * scale;
		const float height = 6.0f * scale;

		const float left = (format & DT_CENTER) > 0 ? rect.left + (rect.right - rect.left - full_width) * 0.5f : rect.left;
		const float top = (format & DT_VCENTER) > 0 ? rect.top + (rect.bottom - rect.top - height) * 0.5f : rect.top;

		if ((format & DT_CALCRECT) > 0)
		{
			rect.right = rect.left + (unsigned)full_width;
			rect.bottom = rect.top + (unsigned)height; // TODO: Support split lines.
		}

		float current_left = left;
		for (unsigned i = 0; i < length; ++i)
		{
			// Top-left origin.
			const float x_0 = current_left;
			const float x_1 = x_0 + width;
			const float x_0_25 = x_0 + (x_1 - x_0) * 0.25f;
			const float x_0_5 = (x_0 + x_1) * 0.5f;
			const float x_0_75 = x_0 + (x_1 - x_0) * 0.75f;

			const float y_0 = top;
			const float y_1 = y_0 + height;
			const float y_0_16 = y_0 + (y_1 - y_0) * 0.16f;
			const float y_0_25 = y_0 + (y_1 - y_0) * 0.25f;
			const float y_0_33 = y_0 + (y_1 - y_0) * 0.33f;
			const float y_0_5 = (y_0 + y_1) * 0.5f;
			const float y_0_66 = y_0 + (y_1 - y_0) * 0.66f;
			const float y_0_75 = y_0 + (y_1 - y_0) * 0.75f;
			const float y_0_83 = y_0 + (y_1 - y_0) * 0.83f;

			current_left += Spacing(text[i]) * scale;

			std::array<simd::vector2, 16> vertices;
			unsigned vertex_count = 0;

			switch (text[i])
			{
			case ' ': break;
			case 'a': Line6(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0_75, y_0_25}, {x_0_75, y_1}, {x_0, y_1}, {x_0, y_0_66}, {x_0_75, y_0_66}); break;
			case 'A': Line4(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0}, {x_1, y_0}, {x_1, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_1, y_0_5}); break;
			case 'b': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}, {x_0_75, y_1}, {x_0_75, y_0_5}, {x_0, y_0_5}); break;
			case 'B': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}, {x_1, y_0_5}, {x_0, y_0_5}); Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_75, y_0}, {x_0_75, y_0_5}); break;
			case 'c': Line4(line, vertices, vertex_count, color, {x_0_75, y_0_25}, {x_0, y_0_25}, {x_0, y_1}, {x_0_75, y_1}); break;
			case 'C': Line4(line, vertices, vertex_count, color, {x_0_75, y_0}, {x_0, y_0}, {x_0, y_1}, {x_0_75, y_1}); break;
			case 'd': Line5(line, vertices, vertex_count, color, {x_0_75, y_0}, {x_0_75, y_1}, {x_0, y_1}, {x_0, y_0_5}, {x_0_75, y_0_5}); break;
			case 'D': Line6(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_5, y_0}, {x_1, y_0_5}, {x_0_5, y_1}, {x_0, y_1}, {x_0, y_0}); break;
			case 'e': Line6(line, vertices, vertex_count, color, {x_0_75, y_1}, {x_0, y_1}, {x_0, y_0_25}, {x_0_75, y_0_25}, {x_0_75, y_0_66}, {x_0, y_0_66}); break;
			case 'E': Line4(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_0_5, y_0_5}); break;
			case 'f': Line3(line, vertices, vertex_count, color, {x_1, y_0}, {x_0_5, y_0}, {x_0_5, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_0_75, y_0_5}); break;
			case 'F': Line3(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0}, {x_0, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_0_5, y_0_5}); break;
			case 'g': Line6(line, vertices, vertex_count, color, {x_0, y_1}, {x_0_75, y_1}, {x_0_75, y_0_25}, {x_0_25, y_0_25}, {x_0_25, y_0_66}, {x_0_75, y_0_66}); break;
			case 'G': Line6(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}, {x_1, y_0_5}, {x_0_5, y_0_5}); break;
			case 'h': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}); Line3(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_0_75, y_0_5}, {x_0_75, y_1}); break;
			case 'H': Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_1, y_0_5}); Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}); Line2(line, vertices, vertex_count, color, {x_1, y_0}, {x_1, y_1}); break;
			case 'i': Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_0, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_0_25}); break;
			case 'I': Line2(line, vertices, vertex_count, color, {x_0_5, y_0}, {x_0_5, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_75, y_0}); Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_0_75, y_1}); break;
			case 'j': Line2(line, vertices, vertex_count, color, {x_0_5, y_0_5}, {x_0_5, y_1}); Line4(line, vertices, vertex_count, color, {x_0, y_0_33}, {x_0_5, y_0_33}, {x_0_5, y_0_83}, {x_0, y_1}); break;
			case 'J': Line4(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_5, y_0}, {x_0_5, y_0_83}, {x_0, y_1}); break;
			case 'k': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}); Line3(line, vertices, vertex_count, color, {x_0_75, y_0_25}, {x_0, y_0_5}, {x_0_75, y_1}); break;
			case 'K': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}); Line3(line, vertices, vertex_count, color, {x_0_75, y_0}, {x_0, y_0_5}, {x_0_75, y_1}); break;
			case 'l': Line4(line, vertices, vertex_count, color, {x_0, y_0_16}, {x_0_5, y_0_16}, {x_0_5, y_1}, {x_0_75, y_1}); break;
			case 'L': Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}); break;
			case 'm': Line4(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0_25}, {x_1, y_0_25}, {x_1, y_1}); Line2(line, vertices, vertex_count, color, {x_0_5, y_0_25}, {x_0_5, y_1}); break;
			case 'M': Line5(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0}, {x_0_5, y_0_5}, {x_1, y_0}, {x_1, y_1}); break;
			case 'n': Line2(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0, y_1}); Line3(line, vertices, vertex_count, color, {x_0, y_0_33}, {x_0_75, y_0_25}, {x_0_75, y_1}); break;
			case 'N': Line4(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0}, {x_1, y_1}, {x_1, y_0}); break;
			case 'o': Line5(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0_75, y_0_25}, {x_0_75, y_1}, {x_0, y_1}, {x_0, y_0_25}); break;
			case 'O': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_1, y_1}, {x_0, y_1}, {x_0, y_0}); break;
			case 'p': Line5(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0_25}, {x_0_75, y_0_25}, {x_0_75, y_0_66}, {x_0, y_0_66}); break;
			case 'P': Line5(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0}, {x_1, y_0}, {x_1, y_0_5}, {x_0, y_0_5}); break;
			case 'q': Line5(line, vertices, vertex_count, color, {x_0_75, y_1}, {x_0_75, y_0_25}, {x_0, y_0_25}, {x_0, y_0_66}, {x_0_75, y_0_66}); break;
			case 'Q': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_1, y_1}, {x_0, y_1}, {x_0, y_0}); Line2(line, vertices, vertex_count, color, {x_0_75, y_0_75}, {x_0_75, y_0_75}); break;
			case 'r': Line4(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0_25}, {x_0_75, y_0_25}, {x_0_75, y_0_33}); break;
			case 'R': Line6(line, vertices, vertex_count, color, {x_0, y_1}, {x_0, y_0}, {x_1, y_0}, {x_1, y_0_5}, {x_0, y_0_5}, {x_1, y_1}); break;
			case 's': Line6(line, vertices, vertex_count, color, {x_0_75, y_0_25}, {x_0, y_0_25}, {x_0, y_0_66}, {x_0_75, y_0_66}, {x_0_75, y_1}, {x_0, y_1}); break;
			case 'S': Line6(line, vertices, vertex_count, color, {x_0_75, y_0}, {x_0, y_0}, {x_0, y_0_5}, {x_1, y_0_5}, {x_1, y_1}, {x_0, y_1}); break;
			case 't': Line2(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0_75, y_0_25}); Line3(line, vertices, vertex_count, color, {x_0_25, y_0}, {x_0_25, y_1}, {x_0_75, y_1}); break;
			case 'T': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}); Line2(line, vertices, vertex_count, color, {x_0_5, y_0}, {x_0_5, y_1}); break;
			case 'u': Line4(line, vertices, vertex_count, color, {x_0, y_0_16}, {x_0, y_1}, {x_0_75, y_1}, {x_0_75, y_0_16}); break;
			case 'U': Line4(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}, {x_1, y_0}); break;
			case 'v': Line3(line, vertices, vertex_count, color, {x_0, y_0_16}, {x_0_5, y_1}, {x_1, y_0_16}); break;
			case 'V': Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_5, y_1}, {x_1, y_0}); break;
			case 'w': Line5(line, vertices, vertex_count, color, {x_0, y_0_16}, {x_0, y_1}, {x_0_5, y_0_5}, {x_1, y_1}, {x_1, y_0_16}); break;
			case 'W': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_1}, {x_0_5, y_0_5}, {x_1, y_1}, {x_1, y_0}); break;
			case 'x': Line2(line, vertices, vertex_count, color, {x_0, y_0_16}, {x_1, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_0_16}); break;
			case 'X': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_0}); break;
			case 'y': Line2(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0_5, y_0_66}); Line2(line, vertices, vertex_count, color, {x_1, y_0_25}, {x_0, y_1}); break;
			case 'Y': Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_0_5, y_0_5}, {x_0_5, y_1}); Line2(line, vertices, vertex_count, color, {x_0_5, y_0_5}, {x_1, y_0}); break;
			case 'z': Line4(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_1, y_0_25}, {x_0, y_1}, {x_1, y_1}); break;
			case 'Z': Line4(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_0, y_1}, {x_1, y_1}); break;
			case ',': 
			case '.': Line2(line, vertices, vertex_count, color, {x_0, y_0_75}, {x_0, y_1}); break;
			case ';': 
			case ':': Line2(line, vertices, vertex_count, color, {x_0, y_0_75}, {x_0, y_1}); Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_0_25}); break;
			case '_': Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_1}); break;
			case '-': Line2(line, vertices, vertex_count, color, {x_0_25, y_0_5}, {x_0_75, y_0_5}); break;
			case '+': Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_1, y_0_5}); Line2(line, vertices, vertex_count, color, {x_0_5, y_0}, {x_0_5, y_1}); break;
			case '=': Line2(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_1, y_0_25}); Line2(line, vertices, vertex_count, color, {x_0, y_0_75}, {x_1, y_0_75}); break;
			case '|': Line2(line, vertices, vertex_count, color, {x_0_5, y_1}, {x_0_5, y_0}); break;
			case '/': Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_0}); break;
			case '\\': Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_1}); break;
			case '%': Line2(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_0}); Line2(line, vertices, vertex_count, color, {x_0, y_0}, {x_0, y_0_25}); Line2(line, vertices, vertex_count, color, {x_1, y_0_75}, {x_1, y_1}); break;
			case '<': Line3(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0_5}, {x_1, y_1}); break;
			case '>': Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0_5}, {x_0, y_1}); break;
			case '(':
			case '[':
			case '{': Line4(line, vertices, vertex_count, color, {x_0_5, y_0}, {x_0, y_0}, {x_0, y_1}, {x_0_5, y_1}); break;
			case ')':
			case ']':
			case '}': Line4(line, vertices, vertex_count, color, {x_0_5, y_0}, {x_1, y_0}, {x_1, y_1}, {x_0_5, y_1}); break;
			case '0': Line6(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_0}, {x_1, y_1}, {x_0, y_1}, {x_0, y_0}, {x_1, y_0}); break;
			case '1': Line3(line, vertices, vertex_count, color, {x_0, y_0_25}, {x_0_5, y_0}, {x_0_5, y_1}); break;
			case '2': Line6(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_1, y_0_5}, {x_0, y_0_5}, {x_0, y_1}, {x_1, y_1}); break;
			case '3': Line4(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_1, y_1}, {x_0, y_1}); Line2(line, vertices, vertex_count, color, {x_0_5, y_0_5}, {x_1, y_0_5}); break;
			case '4': Line4(line, vertices, vertex_count, color, {x_0_75, y_1}, {x_0_75, y_0}, {x_0, y_0_75}, {x_1, y_0_75}); break;
			case '5': Line6(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0}, {x_0, y_0_5}, {x_1, y_0_5}, {x_1, y_1}, {x_0, y_1}); break;
			case '6': Line6(line, vertices, vertex_count, color, {x_1, y_0}, {x_0, y_0}, {x_0, y_1}, {x_1, y_1}, {x_1, y_0_5}, {x_0, y_0_5}); break;
			case '7': Line3(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_0, y_1}); break;
			case '8': Line5(line, vertices, vertex_count, color, {x_0, y_0}, {x_1, y_0}, {x_1, y_1}, {x_0, y_1}, {x_0, y_0}); Line2(line, vertices, vertex_count, color, {x_0, y_0_5}, {x_1, y_0_5}); break;
			case '9': Line6(line, vertices, vertex_count, color, {x_0, y_1}, {x_1, y_1}, {x_1, y_0}, {x_0, y_0}, {x_0, y_0_5}, {x_1, y_0_5}); break;
			}
		}

		line->End();
	}

	void Font::OnLostDevice()
	{
		line->OnLostDevice();
	}

	void Font::OnResetDevice()
	{
		line->OnResetDevice();
	}

	void Font::DrawTextW(int size, LPCWSTR pString, Rect* pRect, DWORD Format, const simd::color& Colour, float scale)
	{
		DrawText(size, WstringToString(pString), *pRect, Format, Colour, scale);
	}



}
