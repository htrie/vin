#pragma once

#include "Common/Simd/Curve.h"
#include "Visual/GUI2/imgui/imgui.h"

#include <functional>

namespace Device::GUI
{
	typedef int CurveControlFlags;

	enum CurveControlFlags_
	{
		CurveControlFlags_None = 0,
		CurveControlFlags_NoRemovePoints = 1 << 0,
		CurveControlFlags_NoNegative = 1 << 1,
		CurveControlFlags_Variance = 1 << 2,
		CurveControlFlags_HideWhenPoppedOut = 1 << 3,
		CurveControlFlags_NoPopOut = 1 << 4,
		CurveControlFlags_NoDecorations = 1 << 5,
	};

	bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t* num_points, size_t max_points, const ImVec2& size, float default_value = 0.0f, float* variance = nullptr, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f);
	inline bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t* num_points, size_t max_points, float default_value = 0.0f, float* variance = nullptr, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f)
	{
		const auto size = ImVec2(ImGui::CalcItemWidth(), ImGui::GetContentRegionAvail().y);
		return CurveControl(label, points, num_points, max_points, size, default_value, variance, flags, toolbar_space);
	}

	inline bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t num_points, const ImVec2& size, float default_value = 0.0f, float* variance = nullptr, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f) { return CurveControl(label, points, &num_points, num_points, size, default_value, variance, flags | CurveControlFlags_NoRemovePoints, toolbar_space); }
	inline bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t num_points, const ImVec2& size, float default_value, CurveControlFlags flags, float toolbar_space = 0.0f) { return CurveControl(label, points, &num_points, num_points, size, default_value, nullptr, flags | CurveControlFlags_NoRemovePoints, toolbar_space); }
	inline bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t num_points, float default_value = 0.0f, float* variance = nullptr, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f) { return CurveControl(label, points, &num_points, num_points, default_value, variance, flags | CurveControlFlags_NoRemovePoints, toolbar_space); }
	inline bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t num_points, float default_value, CurveControlFlags flags, float toolbar_space = 0.0f) { return CurveControl(label, points, &num_points, num_points, default_value, nullptr, flags | CurveControlFlags_NoRemovePoints, toolbar_space); }

	template<size_t N> bool CurveControl(const char* label, simd::GenericCurve<N>& curve, const ImVec2& size, float default_value = 0.0f, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f)
	{
		bool changed = false;
		if (curve.points.size() < 2)
		{
			curve.RescaleTime(0.0f, 1.0f);
			changed = true;
		}

		simd::CurveControlPoint t[N];
		for (size_t a = 0; a < curve.points.size(); a++)
			t[a] = curve.points[a];

		size_t num_points = curve.points.size();
		if (CurveControl(label, t, &num_points, N, size, default_value, &curve.variance, flags, toolbar_space))
		{
			curve.points.clear();
			for (size_t a = 0; a < num_points; a++)
				curve.points.push_back(t[a]);

			changed = true;
		}

		return changed;
	}

	template<size_t N> bool CurveControl(const char* label, simd::GenericCurve<N>& curve, float default_value = 0.0f, CurveControlFlags flags = CurveControlFlags_None, float toolbar_space = 0.0f)
	{
		bool changed = false;
		if (curve.points.size() < 2)
		{
			curve.RescaleTime(0.0f, 1.0f);
			changed = true;
		}

		simd::CurveControlPoint t[N];
		for (size_t a = 0; a < curve.points.size(); a++)
			t[a] = curve.points[a];

		size_t num_points = curve.points.size();
		if (CurveControl(label, t, &num_points, N, default_value, &curve.variance, flags, toolbar_space))
		{
			curve.points.clear();
			for (size_t a = 0; a < num_points; a++)
				curve.points.push_back(t[a]);

			changed = true;
		}

		return changed;
	}

	bool CurveColorControl(const char* label, simd::CurveControlPoint*(&points)[4], size_t(&num_points)[4], size_t max_points, CurveControlFlags flags = CurveControlFlags_None);

	template<size_t N> bool CurveColorControl(const char* label, simd::GenericCurve<N>(&RGBA)[4], CurveControlFlags flags = CurveControlFlags_None)
	{
		bool changed = false;
		for (size_t a = 0; a < 4; a++)
		{
			if (RGBA[a].points.size() < 2)
			{
				RGBA[a].RescaleTime(0.0f, 1.0f);
				changed = true;
			}
		}

		simd::CurveControlPoint t_r[N];
		simd::CurveControlPoint t_g[N];
		simd::CurveControlPoint t_b[N];
		simd::CurveControlPoint t_a[N];

		simd::CurveControlPoint* t_rgba[4] = { t_r, t_g, t_b, t_a };
		size_t num_points[4] = { 0,0,0,0 };
		for (size_t a = 0; a < 4; a++)
		{
			for (size_t b = 0; b < RGBA[a].points.size(); b++)
				t_rgba[a][b] = RGBA[a].points[b];

			num_points[a] = RGBA[a].points.size();
		}

		if (CurveColorControl(label, t_rgba, num_points, N, flags))
		{
			for (size_t a = 0; a < 4; a++)
			{
				RGBA[a].points.clear();
				for (size_t b = 0; b < num_points[a]; b++)
					RGBA[a].points.push_back(t_rgba[a][b]);
			}

			changed = true;
		}

		return changed;	
	}

	template<size_t N> bool CurveColorControl(const char* label, simd::GenericCurve<N>& R, simd::GenericCurve<N>& G, simd::GenericCurve<N>& B, simd::GenericCurve<N>& A, CurveControlFlags flags = CurveControlFlags_None)
	{
		simd::GenericCurve<N> t[4] = { R, G, B, A };
		bool changed = false;

		if (CurveColorControl(label, t, flags))
		{
			changed = true;
			R = t[0];
			G = t[1];
			B = t[2];
			A = t[3];
		}

		return changed;
	}

	class CurveSelector
	{
	private:
		char buffer[256];
		size_t count;
		size_t stage;
		size_t default_selected;
		ImVec2 size;

	public:
		CurveSelector(const char* label, size_t count, const ImVec2& size = ImVec2(0.0f, 0.0f), size_t default_selected = 0);
		CurveSelector(const char* label, size_t count, size_t default_selected) : CurveSelector(label, count, ImVec2(0.0f, 0.0f), default_selected) {}

		bool Step();
	};

	// Returns whether the last CurveControl has its own window
	bool IsCurveControlPoppedOut();
}