#include "mmd_animation.h"

#include "godot_cpp/core/class_db.hpp"

using namespace godot;

void MMDAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_data"), &MMDAnimation::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &MMDAnimation::set_data);
	ClassDB::bind_method(D_METHOD("get_duration"), &MMDAnimation::get_duration);
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &MMDAnimation::set_duration);
	ClassDB::bind_method(D_METHOD("get_model_name"), &MMDAnimation::get_model_name);
	ClassDB::bind_method(D_METHOD("set_model_name", "model_name"), &MMDAnimation::set_model_name);
	ClassDB::bind_method(D_METHOD("get_motion_name"), &MMDAnimation::get_motion_name);
	ClassDB::bind_method(D_METHOD("set_motion_name", "motion_name"), &MMDAnimation::set_motion_name);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "model_name"), "set_model_name", "get_model_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "motion_name"), "set_motion_name", "get_motion_name");
}

PackedByteArray MMDAnimation::get_data() const {
	return data;
}

void MMDAnimation::set_data(const PackedByteArray &p_data) {
	data = p_data;
}

double MMDAnimation::get_duration() const {
	return duration;
}

void MMDAnimation::set_duration(double p_duration) {
	duration = p_duration;
}

String MMDAnimation::get_model_name() const {
	return model_name;
}

void MMDAnimation::set_model_name(const String &p_name) {
	model_name = p_name;
}

String MMDAnimation::get_motion_name() const {
	return motion_name;
}

void MMDAnimation::set_motion_name(const String &p_name) {
	motion_name = p_name;
}
