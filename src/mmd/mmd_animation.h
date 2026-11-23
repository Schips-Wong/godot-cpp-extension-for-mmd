#pragma once

#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/string.hpp"

using namespace godot;

// A VMD motion imported by VmdImporter. The raw file is kept so MMDAnimationPlayer can parse it
// once at runtime (the reference keeps the same bytes inside its VMDResource).
class MMDAnimation : public Resource {
	GDCLASS(MMDAnimation, Resource)

protected:
	static void _bind_methods();

	PackedByteArray data;
	double duration = 0.0;
	String model_name;
	String motion_name;

public:
	PackedByteArray get_data() const;
	void set_data(const PackedByteArray &p_data);

	double get_duration() const;
	void set_duration(double p_duration);

	String get_model_name() const;
	void set_model_name(const String &p_name);

	String get_motion_name() const;
	void set_motion_name(const String &p_name);
};
