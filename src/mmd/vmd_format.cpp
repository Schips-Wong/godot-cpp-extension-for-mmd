#include "vmd_format.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// Cubic bezier helpers shared by every channel. The curve is the one MMD stores as four
// control values per axis: (0,0), (ax, ay), (bx, by), (1,1).
float sample_curve(float p_a, float p_b, float p_t) {
	const float inverse = 1.0f - p_t;
	return 3.0f * inverse * inverse * p_t * p_a + 3.0f * inverse * p_t * p_t * p_b + p_t * p_t * p_t;
}

float sample_curve_derivative(float p_a, float p_b, float p_t) {
	const float inverse = 1.0f - p_t;
	return 3.0f * inverse * inverse * p_a + 6.0f * inverse * p_t * (p_b - p_a) + 3.0f * p_t * p_t * (1.0f - p_b);
}

float solve_curve_x(float p_a, float p_b, float p_x, float p_epsilon) {
	float t = p_x;
	for (int32_t i = 0; i < 8; i++) {
		const float estimate = sample_curve(p_a, p_b, t);
		if (std::abs(estimate - p_x) < p_epsilon) {
			return t;
		}
		const float derivative = sample_curve_derivative(p_a, p_b, t);
		if (std::abs(derivative) < p_epsilon) {
			break;
		}
		t -= (estimate - p_x) / derivative;
	}

	// Newton failed to converge (flat control points): fall back to bisection.
	float low = 0.0f;
	float high = 1.0f;
	t = p_x;
	for (int32_t i = 0; i < 32; i++) {
		const float estimate = sample_curve(p_a, p_b, t);
		if (std::abs(estimate - p_x) < p_epsilon) {
			return t;
		}
		if (estimate < p_x) {
			low = t;
		} else {
			high = t;
		}
		t = (low + high) * 0.5f;
	}
	return t;
}

// VMD stores names as Shift-JIS. Windows can convert it natively; on other platforms the
// bytes are decoded as UTF-8, which is what some non MMD tools write.
String decode_shift_jis(const uint8_t *p_data, int64_t p_length) {
	if (p_length <= 0) {
		return String();
	}
#if defined(_WIN32)
	wchar_t buffer[64];
	const int32_t count = MultiByteToWideChar(932, 0, reinterpret_cast<const char *>(p_data),
			static_cast<int32_t>(p_length), buffer, 64);
	if (count > 0) {
		return String::utf16(reinterpret_cast<const char16_t *>(buffer), count);
	}
#endif
	return String::utf8(reinterpret_cast<const char *>(p_data), p_length);
}

int8_t byte_to_signed(uint32_t p_value) {
	return static_cast<int8_t>(p_value & 0xFF);
}

float normalize_interpolator_value(uint32_t p_value) {
	return static_cast<float>(byte_to_signed(p_value)) / 127.0f;
}

// Bounds checked reader over the file bytes.
class VmdReader {
public:
	VmdReader(const uint8_t *p_data, int64_t p_size) :
			data(p_data), size(p_size) {}

	bool ok() const { return !failed; }
	int64_t remaining() const { return size - offset; }
	bool has(int64_t p_bytes) const { return !failed && offset + p_bytes <= size; }

	void skip(int64_t p_bytes) {
		if (!has(p_bytes)) {
			failed = true;
			return;
		}
		offset += p_bytes;
	}

	uint8_t u8() {
		if (!has(1)) {
			failed = true;
			return 0;
		}
		return data[offset++];
	}

	uint32_t u32() {
		if (!has(4)) {
			failed = true;
			return 0;
		}
		uint32_t value = 0;
		memcpy(&value, data + offset, sizeof(value));
		offset += 4;
		return value;
	}

	int32_t i32() {
		if (!has(4)) {
			failed = true;
			return 0;
		}
		int32_t value = 0;
		memcpy(&value, data + offset, sizeof(value));
		offset += 4;
		return value;
	}

	float f32() {
		if (!has(4)) {
			failed = true;
			return 0.0f;
		}
		float value = 0.0f;
		memcpy(&value, data + offset, sizeof(value));
		offset += 4;
		return value;
	}

	// Components are read in file order on purpose: passing several f32() calls as constructor
	// arguments leaves the evaluation order unspecified and MSVC evaluates right to left.
	Vector3 v3() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		return Vector3(x, y, z);
	}

	Vector3 v3_flip_z() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		return Vector3(x, y, -z);
	}

	Vector3 v3_flip_xy() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		return Vector3(-x, -y, z);
	}

	Quaternion q_flip_xy() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		const float w = f32();
		return Quaternion(-x, -y, z, w);
	}

	String name(int64_t p_bytes) {
		if (!has(p_bytes)) {
			failed = true;
			return String();
		}
		const uint8_t *start = data + offset;
		int64_t length = 0;
		while (length < p_bytes && start[length] != 0) {
			length++;
		}
		offset += p_bytes;
		return decode_shift_jis(start, length);
	}

	bool failed = false;

private:
	const uint8_t *data = nullptr;
	int64_t size = 0;
	int64_t offset = 0;
};

// One bone key frame reads four interpolators that each span 4 int32 values, only the low byte
// of each is used: bytes 0/4/8/12 for x, 1/5/9/13 for y, 2/6/10/14 for z, 3/7/11/15 for r.
VmdInterpolator read_bone_interpolator(VmdReader &p_reader) {
	const uint32_t a = p_reader.u32();
	const uint32_t b = p_reader.u32();
	const uint32_t c = p_reader.u32();
	const uint32_t d = p_reader.u32();
	VmdInterpolator interpolator;
	interpolator.ax = normalize_interpolator_value(a);
	interpolator.ay = normalize_interpolator_value(b);
	interpolator.bx = normalize_interpolator_value(c);
	interpolator.by = normalize_interpolator_value(d);
	return interpolator;
}

// Camera interpolators are stored as four bytes packed inside one int32.
VmdInterpolator read_camera_interpolator(VmdReader &p_reader) {
	const uint32_t value = p_reader.u32();
	VmdInterpolator interpolator;
	interpolator.ax = normalize_interpolator_value(value);
	interpolator.bx = normalize_interpolator_value(value >> 8);
	interpolator.ay = normalize_interpolator_value(value >> 16);
	interpolator.by = normalize_interpolator_value(value >> 24);
	return interpolator;
}

template <typename T>
void sort_by_frame(std::vector<T> &r_keys) {
	std::stable_sort(r_keys.begin(), r_keys.end(), [](const T &p_left, const T &p_right) {
		return p_left.frame < p_right.frame;
	});
}

template <typename Track>
Track *find_or_create_track(std::vector<Track> &r_tracks, std::map<std::string, size_t> &r_lookup, const String &p_name) {
	const std::string key = p_name.utf8().get_data();
	std::map<std::string, size_t>::const_iterator found = r_lookup.find(key);
	if (found != r_lookup.end()) {
		return &r_tracks[found->second];
	}
	const size_t index = r_tracks.size();
	r_tracks.push_back(Track());
	r_tracks.back().name = p_name;
	r_lookup[key] = index;
	return &r_tracks.back();
}

} // namespace

float VmdInterpolator::sample(float p_factor) const {
	if (p_factor <= 0.0f) {
		return 0.0f;
	}
	if (p_factor >= 1.0f) {
		return 1.0f;
	}
	return sample_curve(ay, by, solve_curve_x(ax, bx, p_factor, 1e-6f));
}

const VmdBoneTrack *VmdAnimationData::find_bone_track(const String &p_name) const {
	for (const VmdBoneTrack &track : bone_tracks) {
		if (track.name == p_name) {
			return &track;
		}
	}
	return nullptr;
}

const VmdMorphTrack *VmdAnimationData::find_morph_track(const String &p_name) const {
	for (const VmdMorphTrack &track : morph_tracks) {
		if (track.name == p_name) {
			return &track;
		}
	}
	return nullptr;
}

const VmdIkTrack *VmdAnimationData::find_ik_track(const String &p_name) const {
	for (const VmdIkTrack &track : ik_tracks) {
		if (track.name == p_name) {
			return &track;
		}
	}
	return nullptr;
}

Error parse_vmd(const PackedByteArray &p_data, VmdAnimationData &r_animation) {
	if (p_data.size() < 54) {
		return ERR_FILE_CORRUPT;
	}

	const uint8_t *bytes = p_data.ptr();
	if (memcmp(bytes, "Vocaloid Motion Data", 20) != 0) {
		return ERR_FILE_UNRECOGNIZED;
	}

	VmdReader reader(bytes, p_data.size());
	reader.skip(30); // "Vocaloid Motion Data 0002" + padding
	r_animation.model_name = reader.name(20);
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}

	int32_t max_frame = 0;
	bool has_frames = false;

	// --- Bone key frames ----------------------------------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		std::map<std::string, size_t> lookup;
		for (int32_t i = 0; i < count && reader.ok(); i++) {
			const String bone_name = reader.name(15);
			VmdBoneKeyFrame key;
			key.frame = reader.i32();
			key.translation = reader.v3_flip_z();
			key.rotation = reader.q_flip_xy();
			key.x = read_bone_interpolator(reader);
			key.y = read_bone_interpolator(reader);
			key.z = read_bone_interpolator(reader);
			key.r = read_bone_interpolator(reader);
			if (!reader.ok()) {
				break;
			}
			VmdBoneTrack *track = find_or_create_track(r_animation.bone_tracks, lookup, bone_name);
			track->keys.push_back(key);
			max_frame = std::max(max_frame, key.frame);
			has_frames = true;
		}
	}
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}
	for (VmdBoneTrack &track : r_animation.bone_tracks) {
		sort_by_frame(track.keys);
	}

	// --- Morph key frames ---------------------------------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		std::map<std::string, size_t> lookup;
		for (int32_t i = 0; i < count && reader.ok(); i++) {
			const String morph_name = reader.name(15);
			VmdMorphKeyFrame key;
			key.frame = reader.i32();
			key.weight = reader.f32();
			if (!reader.ok()) {
				break;
			}
			VmdMorphTrack *track = find_or_create_track(r_animation.morph_tracks, lookup, morph_name);
			track->keys.push_back(key);
			max_frame = std::max(max_frame, key.frame);
			has_frames = true;
		}
	}
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}
	for (VmdMorphTrack &track : r_animation.morph_tracks) {
		sort_by_frame(track.keys);
	}

	// --- Camera key frames --------------------------------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		r_animation.camera_frames.reserve(static_cast<size_t>(count));
		for (int32_t i = 0; i < count && reader.ok(); i++) {
			VmdCameraKeyFrame key;
			key.frame = reader.i32();
			key.distance = reader.f32();
			key.position = reader.v3_flip_z();
			key.rotation = reader.v3_flip_xy();
			key.mx = read_camera_interpolator(reader);
			key.my = read_camera_interpolator(reader);
			key.mz = read_camera_interpolator(reader);
			key.r = read_camera_interpolator(reader);
			key.d = read_camera_interpolator(reader);
			key.f = read_camera_interpolator(reader);
			key.fov = static_cast<float>(reader.i32());
			key.orthographic = reader.u8() != 0;
			if (!reader.ok()) {
				break;
			}
			r_animation.camera_frames.push_back(key);
		}
	}
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}
	sort_by_frame(r_animation.camera_frames);

	// --- Light key frames ---------------------------------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		r_animation.light_frames.reserve(static_cast<size_t>(count));
		for (int32_t i = 0; i < count && reader.ok(); i++) {
			VmdLightKeyFrame key;
			key.frame = reader.i32();
			key.color = reader.v3();
			key.position = reader.v3_flip_z();
			if (!reader.ok()) {
				break;
			}
			r_animation.light_frames.push_back(key);
		}
	}
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}
	sort_by_frame(r_animation.light_frames);

	// --- Shadow key frames (parsed and dropped) -----------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		reader.skip(static_cast<int64_t>(count) * 9);
	}

	// --- IK on / off key frames ---------------------------------------------
	if (reader.remaining() > 0) {
		const int32_t count = reader.i32();
		if (count < 0) {
			return ERR_FILE_CORRUPT;
		}
		std::map<std::string, size_t> lookup;
		for (int32_t i = 0; i < count && reader.ok(); i++) {
			const int32_t frame = reader.i32();
			reader.u8(); // model display flag
			const int32_t ik_count = reader.i32();
			if (ik_count < 0) {
				return ERR_FILE_CORRUPT;
			}
			for (int32_t ik = 0; ik < ik_count && reader.ok(); ik++) {
				const String ik_name = reader.name(20);
				VmdIkKeyFrame key;
				key.frame = frame;
				key.enable = reader.u8() != 0;
				if (!reader.ok()) {
					break;
				}
				VmdIkTrack *track = find_or_create_track(r_animation.ik_tracks, lookup, ik_name);
				track->keys.push_back(key);
			}
		}
	}
	if (!reader.ok()) {
		return ERR_FILE_CORRUPT;
	}
	for (VmdIkTrack &track : r_animation.ik_tracks) {
		sort_by_frame(track.keys);
	}

	r_animation.duration = has_frames ? static_cast<double>(max_frame) / 30.0 : 0.0;
	return OK;
}
