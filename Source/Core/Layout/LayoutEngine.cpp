#include "LayoutEngine.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/ElementText.h"
#include "../../../Include/RmlUi/Core/FontEngineInterface.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "../../../Include/RmlUi/Core/StringUtilities.h"
#include "../../../Include/RmlUi/Core/Traits.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <yoga/Yoga.h>

namespace Rml {

namespace {

	static YGFlexDirection ToYogaFlexDirection(Style::FlexDirection v)
	{
		switch (v)
		{
		case Style::FlexDirection::Row: return YGFlexDirectionRow;
		case Style::FlexDirection::RowReverse: return YGFlexDirectionRowReverse;
		case Style::FlexDirection::Column: return YGFlexDirectionColumn;
		case Style::FlexDirection::ColumnReverse: return YGFlexDirectionColumnReverse;
		}
		return YGFlexDirectionRow;
	}

	static YGWrap ToYogaWrap(Style::FlexWrap v)
	{
		switch (v)
		{
		case Style::FlexWrap::Nowrap: return YGWrapNoWrap;
		case Style::FlexWrap::Wrap: return YGWrapWrap;
		case Style::FlexWrap::WrapReverse: return YGWrapWrapReverse;
		}
		return YGWrapNoWrap;
	}

	static YGJustify ToYogaJustify(Style::JustifyContent v)
	{
		switch (v)
		{
		case Style::JustifyContent::FlexStart: return YGJustifyFlexStart;
		case Style::JustifyContent::FlexEnd: return YGJustifyFlexEnd;
		case Style::JustifyContent::Center: return YGJustifyCenter;
		case Style::JustifyContent::SpaceBetween: return YGJustifySpaceBetween;
		case Style::JustifyContent::SpaceAround: return YGJustifySpaceAround;
		case Style::JustifyContent::SpaceEvenly: return YGJustifySpaceEvenly;
		}
		return YGJustifyFlexStart;
	}

	static YGAlign ToYogaAlignContent(Style::AlignContent v)
	{
		switch (v)
		{
		case Style::AlignContent::FlexStart: return YGAlignFlexStart;
		case Style::AlignContent::FlexEnd: return YGAlignFlexEnd;
		case Style::AlignContent::Center: return YGAlignCenter;
		case Style::AlignContent::SpaceBetween: return YGAlignSpaceBetween;
		case Style::AlignContent::SpaceAround: return YGAlignSpaceAround;
		case Style::AlignContent::SpaceEvenly: return YGAlignSpaceEvenly;
		case Style::AlignContent::Stretch: return YGAlignStretch;
		}
		return YGAlignStretch;
	}

	static YGAlign ToYogaAlignItems(Style::AlignItems v)
	{
		switch (v)
		{
		case Style::AlignItems::FlexStart: return YGAlignFlexStart;
		case Style::AlignItems::FlexEnd: return YGAlignFlexEnd;
		case Style::AlignItems::Center: return YGAlignCenter;
		case Style::AlignItems::Baseline: return YGAlignBaseline;
		case Style::AlignItems::Stretch: return YGAlignStretch;
		}
		return YGAlignStretch;
	}

	static YGAlign ToYogaAlignSelf(Style::AlignSelf v)
	{
		switch (v)
		{
		case Style::AlignSelf::Auto: return YGAlignAuto;
		case Style::AlignSelf::FlexStart: return YGAlignFlexStart;
		case Style::AlignSelf::FlexEnd: return YGAlignFlexEnd;
		case Style::AlignSelf::Center: return YGAlignCenter;
		case Style::AlignSelf::Baseline: return YGAlignBaseline;
		case Style::AlignSelf::Stretch: return YGAlignStretch;
		}
		return YGAlignAuto;
	}

	static YGPositionType ToYogaPositionType(Style::Position v)
	{
		switch (v)
		{
		case Style::Position::Static: return YGPositionTypeStatic;
		case Style::Position::Relative: return YGPositionTypeRelative;
		case Style::Position::Absolute: return YGPositionTypeAbsolute;
		case Style::Position::Fixed:
			// Yoga does not have a 'fixed' position type, we approximate with absolute.
			return YGPositionTypeAbsolute;
		}
		return YGPositionTypeRelative;
	}

	static YGDirection ToYogaDirection(Style::Direction v)
	{
		switch (v)
		{
		case Style::Direction::Auto: return YGDirectionInherit;
		case Style::Direction::Ltr: return YGDirectionLTR;
		case Style::Direction::Rtl: return YGDirectionRTL;
		}
		return YGDirectionLTR;
	}

	static YGOverflow ToYogaOverflow(Style::Overflow v)
	{
		switch (v)
		{
		case Style::Overflow::Visible: return YGOverflowVisible;
		case Style::Overflow::Hidden: return YGOverflowHidden;
		case Style::Overflow::Scroll: return YGOverflowScroll;
		case Style::Overflow::Auto:
			// Yoga's overflow doesn't distinguish auto/scroll; treat auto as scroll.
			return YGOverflowScroll;
		}
		return YGOverflowVisible;
	}

	static YGBoxSizing ToYogaBoxSizing(Style::BoxSizing v)
	{
		switch (v)
		{
		case Style::BoxSizing::ContentBox: return YGBoxSizingContentBox;
		case Style::BoxSizing::BorderBox: return YGBoxSizingBorderBox;
		}
		return YGBoxSizingContentBox;
	}

	static void SetYogaDimension(YGNodeRef node, YGDimension dim, Style::LengthPercentageAuto value)
	{
		// Yoga C API does not have a generic 'SetDimension', we must dispatch manually.
		if (dim == YGDimensionWidth)
		{
			switch (value.type)
			{
			case Style::LengthPercentageAuto::Auto: YGNodeStyleSetWidthAuto(node); break;
			case Style::LengthPercentageAuto::Length: YGNodeStyleSetWidth(node, value.value); break;
			case Style::LengthPercentageAuto::Percentage: YGNodeStyleSetWidthPercent(node, value.value); break;
			}
		}
		else // YGDimensionHeight
		{
			switch (value.type)
			{
			case Style::LengthPercentageAuto::Auto: YGNodeStyleSetHeightAuto(node); break;
			case Style::LengthPercentageAuto::Length: YGNodeStyleSetHeight(node, value.value); break;
			case Style::LengthPercentageAuto::Percentage: YGNodeStyleSetHeightPercent(node, value.value); break;
			}
		}
	}

	static void SetYogaMinDimension(YGNodeRef node, YGDimension dim, Style::LengthPercentage value)
	{
		if (dim == YGDimensionWidth)
		{
			switch (value.type)
			{
			case Style::LengthPercentage::Length: YGNodeStyleSetMinWidth(node, value.value); break;
			case Style::LengthPercentage::Percentage: YGNodeStyleSetMinWidthPercent(node, value.value); break;
			}
		}
		else // YGDimensionHeight
		{
			switch (value.type)
			{
			case Style::LengthPercentage::Length: YGNodeStyleSetMinHeight(node, value.value); break;
			case Style::LengthPercentage::Percentage: YGNodeStyleSetMinHeightPercent(node, value.value); break;
			}
		}
	}

	static void SetYogaMaxDimension(YGNodeRef node, YGDimension dim, Style::LengthPercentage value)
	{
		// RmlUi converts 'none' to FLT_MAX. Yoga treats undefined max as no constraint.
		// NOTE: If you are reusing nodes, simply 'returning' here might leave
		// a previous Max dimension set. If you need to clear it, typically you
		// rely on Yoga's default or set it to undefined (though the C API for
		// explicit "Undefined" setters is limited in some versions).
		if (value.type == Style::LengthPercentage::Length && value.value > 1.0e20f)
			return;

		if (dim == YGDimensionWidth)
		{
			switch (value.type)
			{
			case Style::LengthPercentage::Length: YGNodeStyleSetMaxWidth(node, value.value); break;
			case Style::LengthPercentage::Percentage: YGNodeStyleSetMaxWidthPercent(node, value.value); break;
			}
		}
		else // YGDimensionHeight
		{
			switch (value.type)
			{
			case Style::LengthPercentage::Length: YGNodeStyleSetMaxHeight(node, value.value); break;
			case Style::LengthPercentage::Percentage: YGNodeStyleSetMaxHeightPercent(node, value.value); break;
			}
		}
	}

	static void SetYogaMargin(YGNodeRef node, YGEdge edge, Style::LengthPercentageAuto value)
	{
		switch (value.type)
		{
		case Style::LengthPercentageAuto::Auto: YGNodeStyleSetMarginAuto(node, edge); break;
		case Style::LengthPercentageAuto::Length: YGNodeStyleSetMargin(node, edge, value.value); break;
		case Style::LengthPercentageAuto::Percentage: YGNodeStyleSetMarginPercent(node, edge, value.value); break;
		}
	}

	static void SetYogaPadding(YGNodeRef node, YGEdge edge, Style::LengthPercentage value)
	{
		switch (value.type)
		{
		case Style::LengthPercentage::Length: YGNodeStyleSetPadding(node, edge, value.value); break;
		case Style::LengthPercentage::Percentage: YGNodeStyleSetPaddingPercent(node, edge, value.value); break;
		}
	}

	static void SetYogaPosition(YGNodeRef node, YGEdge edge, Style::LengthPercentageAuto value)
	{
		switch (value.type)
		{
		case Style::LengthPercentageAuto::Auto: YGNodeStyleSetPositionAuto(node, edge); break;
		case Style::LengthPercentageAuto::Length: YGNodeStyleSetPosition(node, edge, value.value); break;
		case Style::LengthPercentageAuto::Percentage: YGNodeStyleSetPositionPercent(node, edge, value.value); break;
		}
	}

	static void SetYogaGap(YGNodeRef node, YGGutter gutter, Style::LengthPercentage value)
	{
		switch (value.type)
		{
		case Style::LengthPercentage::Length: YGNodeStyleSetGap(node, gutter, value.value); break;
		case Style::LengthPercentage::Percentage: YGNodeStyleSetGapPercent(node, gutter, value.value); break;
		}
	}

	static float YogaBaselineFunc(YGNodeConstRef node, float width, float height)
	{
		Element* element = reinterpret_cast<Element*>(YGNodeGetContext(node));
		if (auto text_element = rmlui_dynamic_cast<ElementText*>(element))
		{
			FontFaceHandle font_face = text_element->GetFontFaceHandle();
			FontMetrics metrics{};

			// Guard: font_face might be null
			if (font_face)
			{
				if (auto* font_engine = GetFontEngineInterface())
					metrics = font_engine->GetFontMetrics(font_face);
			}

			const float line_height = std::max(0.f, text_element->GetLineHeight());

			// FIX: Use std::abs to treat descent as a positive height magnitude
			const float font_height = std::max(0.f, metrics.ascent + std::abs(metrics.descent));

			const float leading = std::max(0.f, line_height - font_height);
			const float baseline = leading * 0.5f + metrics.ascent;

			return baseline;
		}
		return height;
	}

	static YGSize YogaMeasureFunc(YGNodeConstRef node, float width, YGMeasureMode width_mode, float height, YGMeasureMode height_mode)
	{
		Element* element = reinterpret_cast<Element*>(YGNodeGetContext(node));
		if (!element)
			return {0, 0};

		// Text nodes
		if (auto text_element = rmlui_dynamic_cast<ElementText*>(element))
		{
			// Determine width constraint for wrapping.
			float wrap_width = 0.f;
			bool has_wrap_width = false;
			if (width_mode == YGMeasureModeExactly || width_mode == YGMeasureModeAtMost)
			{
				wrap_width = std::max(0.f, width);
				has_wrap_width = true;
			}

			String line;
			int line_length = 0;
			float line_width = 0.f;
			int line_begin = 0;
			int num_lines = 0;
			float max_line_width = 0.f;

			// Guard against infinite loops for very small constraints.
			const int max_iterations = 4096;
			for (int it = 0; it < max_iterations; it++)
			{
				const float max_width = has_wrap_width ? wrap_width : std::numeric_limits<float>::infinity();
				const bool reached_end = text_element->GenerateLine(line, line_length, line_width, line_begin, max_width, 0.f, true, true, true);

				if (line_length <= 0)
				{
					// Nothing consumed; treat as end to avoid stalling.
					if (reached_end)
						break;
					// If we couldn't consume anything, force termination.
					break;
				}

				max_line_width = std::max(max_line_width, line_width);
				num_lines += 1;
				line_begin += line_length;

				if (reached_end)
					break;
			}

			// Ensure at least one line height for empty strings.
			if (num_lines == 0)
				num_lines = 1;

			FontFaceHandle font_face = text_element->GetFontFaceHandle();
			FontMetrics metrics = {}; // Default init to 0

			// Guard: Check for valid font face and font engine
			if (font_face)
			{
				if (auto* font_engine = GetFontEngineInterface())
					metrics = font_engine->GetFontMetrics(font_face);
			}

			float tight_height = std::max(0.f, metrics.ascent + std::abs(metrics.descent));

			float measured_width = max_line_width;
			float measured_height = num_lines * tight_height;

			// Respect the measure modes.
			if (width_mode == YGMeasureModeExactly)
				measured_width = std::max(0.f, width);
			else if (width_mode == YGMeasureModeAtMost)
				measured_width = std::min(measured_width, std::max(0.f, width));

			if (height_mode == YGMeasureModeExactly)
				measured_height = std::max(0.f, height);
			else if (height_mode == YGMeasureModeAtMost)
				measured_height = std::min(measured_height, std::max(0.f, height));

			return {measured_width, measured_height};
		}

		// Replaced elements (images, etc.)
		Vector2f intrinsic_dimensions;
		float intrinsic_ratio = 0.f;
		if (element->GetIntrinsicDimensions(intrinsic_dimensions, intrinsic_ratio))
		{
			float measured_width = intrinsic_dimensions.x;
			float measured_height = intrinsic_dimensions.y;

			const bool width_definite = (width_mode == YGMeasureModeExactly || width_mode == YGMeasureModeAtMost);
			const bool height_definite = (height_mode == YGMeasureModeExactly || height_mode == YGMeasureModeAtMost);

			if (width_mode == YGMeasureModeExactly)
				measured_width = std::max(0.f, width);
			else if (width_mode == YGMeasureModeAtMost)
				measured_width = std::min(measured_width, std::max(0.f, width));

			if (height_mode == YGMeasureModeExactly)
				measured_height = std::max(0.f, height);
			else if (height_mode == YGMeasureModeAtMost)
				measured_height = std::min(measured_height, std::max(0.f, height));

			// If only one dimension is constrained/known, preserve intrinsic ratio when possible.
			if (intrinsic_ratio > 0.f)
			{
				if (width_definite && !height_definite)
					measured_height = measured_width / intrinsic_ratio;
				else if (height_definite && !width_definite)
					measured_width = measured_height * intrinsic_ratio;
			}

			return {std::max(0.f, measured_width), std::max(0.f, measured_height)};
		}

		return {0, 0};
	}

	static void ApplyYogaStyleToNode(YGNodeRef node, Element* element)
	{
		const ComputedValues& c = element->GetComputedValues();

		// This engine is a pure flexbox renderer: treat everything as 'display:flex' except 'display:none'.
		const bool is_display_none = (c.display() == Style::Display::None);
		YGNodeStyleSetDisplay(node, is_display_none ? YGDisplayNone : YGDisplayFlex);

		YGNodeStyleSetBoxSizing(node, ToYogaBoxSizing(c.box_sizing()));
		YGNodeStyleSetFlexDirection(node, ToYogaFlexDirection(c.flex_direction()));
		YGNodeStyleSetFlexWrap(node, ToYogaWrap(c.flex_wrap()));
		YGNodeStyleSetJustifyContent(node, ToYogaJustify(c.justify_content()));
		YGNodeStyleSetAlignContent(node, ToYogaAlignContent(c.align_content()));
		YGNodeStyleSetAlignItems(node, ToYogaAlignItems(c.align_items()));
		YGNodeStyleSetAlignSelf(node, ToYogaAlignSelf(c.align_self()));
		YGNodeStyleSetFlexGrow(node, c.flex_grow());
		YGNodeStyleSetFlexShrink(node, c.flex_shrink());

		{
			const Style::FlexBasis fb = c.flex_basis();
			switch (fb.type)
			{
			case Style::LengthPercentageAuto::Auto: YGNodeStyleSetFlexBasisAuto(node); break;
			case Style::LengthPercentageAuto::Length: YGNodeStyleSetFlexBasis(node, fb.value); break;
			case Style::LengthPercentageAuto::Percentage: YGNodeStyleSetFlexBasisPercent(node, fb.value); break;
			}
		}

		SetYogaDimension(node, YGDimensionWidth, c.width());
		SetYogaDimension(node, YGDimensionHeight, c.height());
		SetYogaMinDimension(node, YGDimensionWidth, c.min_width());
		SetYogaMinDimension(node, YGDimensionHeight, c.min_height());
		SetYogaMaxDimension(node, YGDimensionWidth, c.max_width());
		SetYogaMaxDimension(node, YGDimensionHeight, c.max_height());

		// Margins (CSS percentages resolve against width; Yoga resolves per spec).
		SetYogaMargin(node, YGEdgeTop, c.margin_top());
		SetYogaMargin(node, YGEdgeRight, c.margin_right());
		SetYogaMargin(node, YGEdgeBottom, c.margin_bottom());
		SetYogaMargin(node, YGEdgeLeft, c.margin_left());

		SetYogaPadding(node, YGEdgeTop, c.padding_top());
		SetYogaPadding(node, YGEdgeRight, c.padding_right());
		SetYogaPadding(node, YGEdgeBottom, c.padding_bottom());
		SetYogaPadding(node, YGEdgeLeft, c.padding_left());

		YGNodeStyleSetBorder(node, YGEdgeTop, (float)c.border_top_width());
		YGNodeStyleSetBorder(node, YGEdgeRight, (float)c.border_right_width());
		YGNodeStyleSetBorder(node, YGEdgeBottom, (float)c.border_bottom_width());
		YGNodeStyleSetBorder(node, YGEdgeLeft, (float)c.border_left_width());

		// Gaps
		SetYogaGap(node, YGGutterRow, c.row_gap());
		SetYogaGap(node, YGGutterColumn, c.column_gap());

		// Positioning
		YGNodeStyleSetPositionType(node, ToYogaPositionType(c.position()));
		SetYogaPosition(node, YGEdgeTop, c.top());
		SetYogaPosition(node, YGEdgeRight, c.right());
		SetYogaPosition(node, YGEdgeBottom, c.bottom());
		SetYogaPosition(node, YGEdgeLeft, c.left());

		// Yoga exposes a single overflow property. Use the most restrictive of the two axes.
		Style::Overflow combined_overflow = c.overflow_x();
		if (c.overflow_y() == Style::Overflow::Hidden || combined_overflow == Style::Overflow::Hidden)
			combined_overflow = Style::Overflow::Hidden;
		else if (c.overflow_y() == Style::Overflow::Scroll || c.overflow_y() == Style::Overflow::Auto ||
			combined_overflow == Style::Overflow::Scroll || combined_overflow == Style::Overflow::Auto)
			combined_overflow = Style::Overflow::Scroll;
		else
			combined_overflow = Style::Overflow::Visible;
		YGNodeStyleSetOverflow(node, ToYogaOverflow(combined_overflow));
	}

	static YGNodeRef BuildYogaTreeRecursive(Element* element, YGConfigRef config)
	{
		YGNodeRef node = YGNodeNewWithConfig(config);
		YGNodeSetContext(node, element);
		ApplyYogaStyleToNode(node, element);

		const bool is_leaf = (element->GetNumChildren() == 0);
		const bool is_text = (rmlui_dynamic_cast<ElementText*>(element) != nullptr);

		Vector2f intrinsic_dimensions;
		float intrinsic_ratio = 0.f;
		const bool is_replaced = element->GetIntrinsicDimensions(intrinsic_dimensions, intrinsic_ratio);

		if (is_leaf && (is_text || is_replaced))
		{
			YGNodeSetMeasureFunc(node, YogaMeasureFunc);

			if (is_text)
			{
				YGNodeSetBaselineFunc(node, YogaBaselineFunc);
			}
		}
		else
		{
			const int num_children = element->GetNumChildren();
			for (int i = 0; i < num_children; i++)
			{
				Element* child = element->GetChild(i);
				if (!child)
					continue;
				YGNodeInsertChild(node, BuildYogaTreeRecursive(child, config), (uint32_t)YGNodeGetChildCount(node));
			}
		}

		// Direction is inherited in Yoga when set to 'inherit'. We map RmlUi direction directly.
		YGNodeStyleSetDirection(node, ToYogaDirection(element->GetComputedValues().direction()));

		return node;
	}

	static void GenerateTextLines(ElementText* text_element, float available_width)
	{
		// Generate line breaks based on the final available width, and store them for rendering.
		text_element->ClearLines();

		const float max_width = std::max(0.f, available_width);

		// Fetch font metrics for baseline placement within the line box.
		FontFaceHandle font_face = text_element->GetFontFaceHandle();
		FontMetrics metrics{};

		// Guard: font_face might be null
		if (font_face)
		{
			if (auto* font_engine = GetFontEngineInterface())
				metrics = font_engine->GetFontMetrics(font_face);
		}

		const float font_height = std::max(0.f, metrics.ascent + std::abs(metrics.descent));
		const float line_height = std::max(0.f, text_element->GetLineHeight());
		const float leading = std::max(0.f, line_height - font_height);
		const float baseline_offset = std::floor(leading * 0.5f + metrics.ascent);

		String line;
		int line_length = 0;
		float line_width = 0.f;
		int line_begin = 0;
		int line_index = 0;

		const int max_iterations = 4096;
		for (int it = 0; it < max_iterations; it++)
		{
			const bool reached_end = text_element->GenerateLine(line, line_length, line_width, line_begin, max_width, 0.f, true, true, true);
			if (line_length <= 0)
			{
				// Ensure an empty line for empty text.
				if (line_index == 0)
					text_element->AddLine(Vector2f(0.f, baseline_offset), String());
				break;
			}

			const float baseline_y = baseline_offset + (float)line_index * line_height;
			text_element->AddLine(Vector2f(0.f, baseline_y), line);
			line_index += 1;
			line_begin += line_length;

			if (reached_end)
				break;
		}
	}

	static void ApplyLayoutRecursive(Element* element, YGNodeRef node, Element* offset_parent, const Vector2f& parent_content_position)
	{
		RMLUI_ASSERT(element && node);

		const float left = YGNodeLayoutGetLeft(node);
		const float top = YGNodeLayoutGetTop(node);
		const float layout_width = YGNodeLayoutGetWidth(node);
		const float layout_height = YGNodeLayoutGetHeight(node);

		// Resolve box edges from Yoga's computed layout.
		const float margin_left = YGNodeLayoutGetMargin(node, YGEdgeLeft);
		const float margin_top = YGNodeLayoutGetMargin(node, YGEdgeTop);
		const float margin_right = YGNodeLayoutGetMargin(node, YGEdgeRight);
		const float margin_bottom = YGNodeLayoutGetMargin(node, YGEdgeBottom);

		const float padding_left = YGNodeLayoutGetPadding(node, YGEdgeLeft);
		const float padding_top = YGNodeLayoutGetPadding(node, YGEdgeTop);
		const float padding_right = YGNodeLayoutGetPadding(node, YGEdgeRight);
		const float padding_bottom = YGNodeLayoutGetPadding(node, YGEdgeBottom);

		const float border_left = YGNodeLayoutGetBorder(node, YGEdgeLeft);
		const float border_top = YGNodeLayoutGetBorder(node, YGEdgeTop);
		const float border_right = YGNodeLayoutGetBorder(node, YGEdgeRight);
		const float border_bottom = YGNodeLayoutGetBorder(node, YGEdgeBottom);

		Box box;
		box.SetEdge(BoxArea::Margin, BoxEdge::Left, margin_left);
		box.SetEdge(BoxArea::Margin, BoxEdge::Top, margin_top);
		box.SetEdge(BoxArea::Margin, BoxEdge::Right, margin_right);
		box.SetEdge(BoxArea::Margin, BoxEdge::Bottom, margin_bottom);
		box.SetEdge(BoxArea::Border, BoxEdge::Left, border_left);
		box.SetEdge(BoxArea::Border, BoxEdge::Top, border_top);
		box.SetEdge(BoxArea::Border, BoxEdge::Right, border_right);
		box.SetEdge(BoxArea::Border, BoxEdge::Bottom, border_bottom);
		box.SetEdge(BoxArea::Padding, BoxEdge::Left, padding_left);
		box.SetEdge(BoxArea::Padding, BoxEdge::Top, padding_top);
		box.SetEdge(BoxArea::Padding, BoxEdge::Right, padding_right);
		box.SetEdge(BoxArea::Padding, BoxEdge::Bottom, padding_bottom);

		const float content_width = std::max(0.f, layout_width - (padding_left + padding_right + border_left + border_right));
		const float content_height = std::max(0.f, layout_height - (padding_top + padding_bottom + border_top + border_bottom));
		box.SetContent(Vector2f(content_width, content_height));

		// Set box and offset.
		// Yoga positions are relative to the parent's content box.
		const Vector2f border_position = parent_content_position + Vector2f(left, top);
		element->SetOffset(border_position, offset_parent, false);
		element->SetBox(box);

		// Layout children.
		const Vector2f element_content_position = box.GetPosition(BoxArea::Content);

		Vector2f content_overflow(0.f, 0.f);

		const uint32_t yoga_child_count = YGNodeGetChildCount(node);
		const int dom_child_count = element->GetNumChildren();
		const int child_count = std::min<int>((int)yoga_child_count, dom_child_count);
		for (int i = 0; i < child_count; i++)
		{
			Element* child = element->GetChild(i);
			YGNodeRef child_node = YGNodeGetChild(node, (uint32_t)i);
			if (!child || !child_node)
				continue;

			ApplyLayoutRecursive(child, child_node, element, Vector2f(0, 0));

			// Expand scrollable content overflow by child's border box.
			const Vector2f child_border_pos_in_parent_border = child->GetRelativeOffset(BoxArea::Border);
			const Vector2f child_border_pos_in_parent_content = child_border_pos_in_parent_border - element_content_position;
			const Vector2f child_border_size = child->GetBox().GetSize(BoxArea::Border);
			content_overflow.x = std::max(content_overflow.x, child_border_pos_in_parent_content.x + child_border_size.x);
			content_overflow.y = std::max(content_overflow.y, child_border_pos_in_parent_content.y + child_border_size.y);
		}

		// Update scrollable overflow rectangle (approximation).
		const Vector2f padding_top_left(padding_left, padding_top);
		const Vector2f padding_bottom_right(padding_right, padding_bottom);
		const Vector2f padding_size = Vector2f(content_width, content_height) + padding_top_left + padding_bottom_right;
		Vector2f scrollable_overflow_size = padding_size;
		scrollable_overflow_size.x = std::max(scrollable_overflow_size.x, padding_top_left.x + content_overflow.x);
		scrollable_overflow_size.y = std::max(scrollable_overflow_size.y, padding_top_left.y + content_overflow.y);
		element->SetScrollableOverflowRectangle(scrollable_overflow_size, false);

		// Finalize text layout.
		if (auto text_element = rmlui_dynamic_cast<ElementText*>(element))
		{
			GenerateTextLines(text_element, content_width);
		}

		// element->OnLayout();
	}

} // namespace

void LayoutEngine::FormatElement(Element* element, Vector2f containing_block)
{
	RMLUI_ASSERT(element && containing_block.x >= 0 && containing_block.y >= 0);

	YGConfigRef config = YGConfigNew();
	YGConfigSetUseWebDefaults(config, true);
	YGConfigSetPointScaleFactor(config, 1.0f);

	// Wrapper node representing the containing block (parent content box).
	YGNodeRef wrapper = YGNodeNewWithConfig(config);
	YGNodeStyleSetDisplay(wrapper, YGDisplayFlex);
	YGNodeStyleSetWidth(wrapper, containing_block.x);
	YGNodeStyleSetHeight(wrapper, containing_block.y);

	YGNodeRef root_node = BuildYogaTreeRecursive(element, config);
	YGNodeInsertChild(wrapper, root_node, 0);

	const YGDirection dir = ToYogaDirection(element->GetComputedValues().direction());
	YGNodeCalculateLayout(wrapper, containing_block.x, containing_block.y, dir == YGDirectionInherit ? YGDirectionLTR : dir);

	// Apply results back to element tree.
	Element* offset_parent = element->GetParentNode();
	Vector2f parent_content_position(0.f, 0.f);
	if (offset_parent)
		parent_content_position = offset_parent->GetBox().GetPosition(BoxArea::Content);

	ApplyLayoutRecursive(element, root_node, offset_parent, parent_content_position);

	// Cleanup yoga tree.
	YGNodeRemoveChild(wrapper, root_node);
	YGNodeFreeRecursive(root_node);
	YGNodeFree(wrapper);
	YGConfigFree(config);

	{
		RMLUI_ZoneScopedN("ClampScrollOffsetRecursive");
		// The size of the scrollable area might have changed, so clamp the scroll offset to avoid scrolling outside the
		// scrollable area. During layouting, we might be changing the scrollable overflow area of the element several
		// times, such as after enabling scrollbars. For this reason, we don't clamp the scroll offset during layouting,
		// as that could inadvertently clamp it to a temporary size. Now that we know the final layout, including the
		// size of each element's scrollable area, we can finally clamp the scroll offset.
		element->ClampScrollOffsetRecursive();
	}
}

} // namespace Rml