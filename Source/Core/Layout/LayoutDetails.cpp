/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "LayoutDetails.h"

#include "../../../Include/RmlUi/Core/Box.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/ElementText.h"
#include "../../../Include/RmlUi/Core/StringUtilities.h"

#include <algorithm>
#include <limits>

namespace Rml {

static inline float ResolveAgainstWidth(Style::LengthPercentageAuto v, float containing_width, float default_value)
{
	// CSS: Percentages for margin and padding resolve against containing block width.
	return ResolveValueOr(v, containing_width, default_value);
}

static inline float ResolveAgainstWidth(Style::LengthPercentage v, float containing_width, float default_value)
{
	return ResolveValueOr(v, containing_width, default_value);
}

static inline float ClampMaxValue(float v)
{
	// RmlUi represents 'none' as FLT_MAX in computed values. Yoga (and our simplified
	// box builder) treats "unset" as no constraint. Clamp very large values to be
	// treated as effectively unconstrained.
	const float large = 1.0e20f;
	if (v >= large)
		return -1.f;
	return v;
}

void LayoutDetails::BuildBox(Box& box, Vector2f containing_block, Element* element, BuildBoxMode box_mode)
{
	box = Box{};

	if (!element)
	{
		box.SetContent(containing_block);
		return;
	}

	const ComputedValues& c = element->GetComputedValues();

	// Resolve edges. Percentages for margin and padding resolve against containing block width.
	const float cb_width = containing_block.x;
	const float cb_height = containing_block.y;

	const float padding_top = ResolveAgainstWidth(c.padding_top(), cb_width, 0.f);
	const float padding_right = ResolveAgainstWidth(c.padding_right(), cb_width, 0.f);
	const float padding_bottom = ResolveAgainstWidth(c.padding_bottom(), cb_width, 0.f);
	const float padding_left = ResolveAgainstWidth(c.padding_left(), cb_width, 0.f);

	const float border_top = (float)c.border_top_width();
	const float border_right = (float)c.border_right_width();
	const float border_bottom = (float)c.border_bottom_width();
	const float border_left = (float)c.border_left_width();

	const float border_padding_x = padding_left + padding_right + border_left + border_right;
	const float border_padding_y = padding_top + padding_bottom + border_top + border_bottom;

	const bool margin_left_auto = (c.margin_left().type == Style::LengthPercentageAuto::Auto);
	const bool margin_right_auto = (c.margin_right().type == Style::LengthPercentageAuto::Auto);
	float margin_left = margin_left_auto ? 0.f : ResolveAgainstWidth(c.margin_left(), cb_width, 0.f);
	float margin_right = margin_right_auto ? 0.f : ResolveAgainstWidth(c.margin_right(), cb_width, 0.f);
	const float margin_top = (c.margin_top().type == Style::LengthPercentageAuto::Auto) ? 0.f : ResolveAgainstWidth(c.margin_top(), cb_width, 0.f);
	const float margin_bottom = (c.margin_bottom().type == Style::LengthPercentageAuto::Auto) ? 0.f : ResolveAgainstWidth(c.margin_bottom(), cb_width, 0.f);

	// Dimensions. Negative content size indicates 'auto'.
	float content_width = ResolveValueOr(c.width(), cb_width, -1.f);
	float content_height = ResolveValueOr(c.height(), cb_height, -1.f);

	if (box_mode == BuildBoxMode::Inline)
	{
		// Inline boxes do not establish a formatting context here; keep dimensions auto.
		content_width = -1.f;
		content_height = -1.f;
		margin_left = 0.f;
		margin_right = 0.f;
	}

	// Box sizing: width/height in border-box includes padding and border.
	if (c.box_sizing() == Style::BoxSizing::BorderBox)
	{
		if (content_width >= 0.f)
			content_width = std::max(0.f, content_width - border_padding_x);
		if (content_height >= 0.f)
			content_height = std::max(0.f, content_height - border_padding_y);
	}

	// Clamp to min/max constraints when definite.
	if (content_width >= 0.f)
	{
		const float min_w = ResolveValue(c.min_width(), cb_width);
		float max_w = ResolveValue(c.max_width(), cb_width);
		max_w = ClampMaxValue(max_w);
		content_width = std::max(content_width, min_w);
		if (max_w >= 0.f)
			content_width = std::min(content_width, max_w);
	}
	if (content_height >= 0.f)
	{
		const float min_h = ResolveValue(c.min_height(), cb_height);
		float max_h = ResolveValue(c.max_height(), cb_height);
		max_h = ClampMaxValue(max_h);
		content_height = std::max(content_height, min_h);
		if (max_h >= 0.f)
			content_height = std::min(content_height, max_h);
	}

	// Horizontal auto margins. When used outside the Yoga layout pass, we only support a small subset:
	// If width is definite and containing block width is known, distribute remaining space.
	if ((margin_left_auto || margin_right_auto) && content_width >= 0.f && cb_width >= 0.f && box_mode == BuildBoxMode::Block)
	{
		const float fixed = content_width + border_padding_x + (margin_left_auto ? 0.f : margin_left) + (margin_right_auto ? 0.f : margin_right);
		const float remaining = cb_width - fixed;
		if (remaining > 0.f)
		{
			if (margin_left_auto && margin_right_auto)
			{
				margin_left = remaining * 0.5f;
				margin_right = remaining * 0.5f;
			}
			else if (margin_left_auto)
				margin_left = remaining;
			else if (margin_right_auto)
				margin_right = remaining;
		}
		// If remaining <= 0, auto margins are treated as 0.
	}

	box.SetContent(Vector2f(content_width, content_height));

	box.SetEdge(BoxArea::Padding, BoxEdge::Top, padding_top);
	box.SetEdge(BoxArea::Padding, BoxEdge::Right, padding_right);
	box.SetEdge(BoxArea::Padding, BoxEdge::Bottom, padding_bottom);
	box.SetEdge(BoxArea::Padding, BoxEdge::Left, padding_left);

	box.SetEdge(BoxArea::Border, BoxEdge::Top, border_top);
	box.SetEdge(BoxArea::Border, BoxEdge::Right, border_right);
	box.SetEdge(BoxArea::Border, BoxEdge::Bottom, border_bottom);
	box.SetEdge(BoxArea::Border, BoxEdge::Left, border_left);

	box.SetEdge(BoxArea::Margin, BoxEdge::Top, margin_top);
	box.SetEdge(BoxArea::Margin, BoxEdge::Right, margin_right);
	box.SetEdge(BoxArea::Margin, BoxEdge::Bottom, margin_bottom);
	box.SetEdge(BoxArea::Margin, BoxEdge::Left, margin_left);
}

String LayoutDetails::GetDebugElementName(Element* element)
{
	if (!element)
		return "nullptr";
	if (!element->GetId().empty())
		return '#' + element->GetId();
	if (auto element_text = rmlui_dynamic_cast<ElementText*>(element))
		return '\"' + StringUtilities::StripWhitespace(element_text->GetText()).substr(0, 20) + '\"';
	return element->GetAddress(false, false);
}

} // namespace Rml
