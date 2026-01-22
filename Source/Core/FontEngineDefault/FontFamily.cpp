#include "FontFamily.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Math.h"
#include "FontFace.h"
#include <limits.h>

namespace Rml {

FontFamily::FontFamily(const String& name) : name(name) {}

FontFamily::~FontFamily()
{
	// Multiple face entries may share memory within a single font family, although only one of them owns it. Here we make sure that all the face
	// destructors are run before all the memory is released. This way we don't leave any hanging references to invalidated memory.
	for (FontFaceEntry& entry : font_faces)
		entry.face.reset();
}

FontFaceHandleDefault* FontFamily::GetFaceHandle(Style::FontStyle style, Style::FontWeight weight, int size)
{
	// First, attempt to find a face with an exact style match.
	int best_weight_dist = INT_MAX;
	FontFace* matching_face = nullptr;
	for (size_t i = 0; i < font_faces.size(); i++)
	{
		FontFace* face = font_faces[i].face.get();

		if (face->GetStyle() != style)
			continue;

		const int dist = Math::Absolute((int)face->GetWeight() - (int)weight);
		if (dist == 0)
		{
			// Direct match for weight, break the loop early.
			matching_face = face;
			break;
		}
		else if (dist < best_weight_dist)
		{
			matching_face = face;
			best_weight_dist = dist;
		}
	}

	// If no face exists with the requested style, fall back to the closest available style.
	// This avoids returning nullptr and ensures that text can still render.
	if (!matching_face)
	{
		int best_style_dist = INT_MAX;
		best_weight_dist = INT_MAX;

		for (size_t i = 0; i < font_faces.size(); i++)
		{
			FontFace* face = font_faces[i].face.get();
			const int style_dist = (face->GetStyle() == style ? 0 : 1);
			const int weight_dist = Math::Absolute((int)face->GetWeight() - (int)weight);

			if (style_dist < best_style_dist || (style_dist == best_style_dist && weight_dist < best_weight_dist))
			{
				matching_face = face;
				best_style_dist = style_dist;
				best_weight_dist = weight_dist;
			}
		}
	}

	if (!matching_face)
		return nullptr;

	// If the requested weight doesn't exist, we may end up selecting a lighter face. In that case,
	// apply a synthetic weight adjustment so the rendered glyphs become visually heavier.
	const int weight_delta = Math::Max((int)weight - (int)matching_face->GetWeight(), 0);

	return matching_face->GetHandle(size, true, weight_delta);
}

FontFace* FontFamily::AddFace(FontFaceHandleFreetype ft_face, Style::FontStyle style, Style::FontWeight weight, UniquePtr<byte[]> face_memory)
{
	auto face = MakeUnique<FontFace>(ft_face, style, weight);
	FontFace* result = face.get();

	font_faces.push_back(FontFaceEntry{std::move(face), std::move(face_memory)});

	return result;
}

void FontFamily::ReleaseFontResources()
{
	for (auto& entry : font_faces)
		entry.face->ReleaseFontResources();
}

} // namespace Rml
