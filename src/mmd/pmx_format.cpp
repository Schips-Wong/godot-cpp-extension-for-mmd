#include "pmx_format.h"

#include <cstring>

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/variant/vector4.hpp"

namespace {

// Bounds checked reader over the file bytes.
class PmxReader {
public:
	PmxReader(const uint8_t *p_data, int64_t p_size) :
			data(p_data), size(p_size) {}

	bool ok() const { return !failed; }
	int64_t position() const { return offset; }
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

	uint16_t u16() {
		if (!has(2)) {
			failed = true;
			return 0;
		}
		uint16_t value = 0;
		memcpy(&value, data + offset, sizeof(value));
		offset += 2;
		return value;
	}

	int16_t i16() {
		if (!has(2)) {
			failed = true;
			return 0;
		}
		int16_t value = 0;
		memcpy(&value, data + offset, sizeof(value));
		offset += 2;
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

	// The components must be read in file order: passing several f32() calls directly as
	// constructor arguments leaves their evaluation order unspecified, and MSVC evaluates
	// right to left, which used to reverse every vector read with these helpers.
	Vector2 v2() {
		const float x = f32();
		const float y = f32();
		return Vector2(x, y);
	}

	Vector3 v3() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		return Vector3(x, y, z);
	}

	Vector4 v4() {
		const float x = f32();
		const float y = f32();
		const float z = f32();
		const float w = f32();
		return Vector4(x, y, z, w);
	}

	// PMX is left handed: negate z (and x/y for rotations) to get Godot's space.
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

	int32_t index_signed(int32_t p_index_size) {
		switch (p_index_size) {
			case 1:
				return static_cast<int32_t>(static_cast<int8_t>(u8()));
			case 2:
				return static_cast<int32_t>(i16());
			case 4:
				return i32();
			default:
				failed = true;
				return -1;
		}
	}

	int32_t index_unsigned(int32_t p_index_size) {
		switch (p_index_size) {
			case 1:
				return u8();
			case 2:
				return static_cast<int32_t>(u16());
			case 4:
				return i32();
			default:
				failed = true;
				return -1;
		}
	}

	String string_utf(bool p_utf8) {
		const int32_t length = i32();
		if (length < 0 || !has(length)) {
			failed = true;
			return String();
		}
		String result;
		if (p_utf8) {
			result = String::utf8(reinterpret_cast<const char *>(data + offset), length);
		} else {
			result = String::utf16(reinterpret_cast<const char16_t *>(data + offset), length / 2);
		}
		offset += length;
		return result;
	}

	bool failed = false;

private:
	const uint8_t *data = nullptr;
	int64_t size = 0;
	int64_t offset = 0;
};

struct PmxGlobals {
	int32_t text_encoding = 0;
	int32_t extra_uv = 0;
	int32_t vertex_index_size = 2;
	int32_t texture_index_size = 2;
	int32_t material_index_size = 2;
	int32_t bone_index_size = 2;
	int32_t morph_index_size = 2;
	int32_t rigid_body_index_size = 2;
};

} // namespace

Error parse_pmx(const PackedByteArray &p_data, PmxModel &r_model) {
	if (p_data.size() < 16) {
		return ERR_FILE_CORRUPT;
	}

	PmxReader reader(p_data.ptr(), p_data.size());

	// "PMX " magic.
	char magic[4] = {};
	for (int32_t i = 0; i < 4; i++) {
		magic[i] = static_cast<char>(reader.u8());
	}
	if (memcmp(magic, "PMX ", 4) != 0) {
		return ERR_FILE_UNRECOGNIZED;
	}

	const float version = reader.f32();
	if (version < 2.0f || version > 2.2f) {
		return ERR_FILE_UNRECOGNIZED;
	}

	const int32_t globals_count = reader.u8();
	if (globals_count < 8) {
		return ERR_FILE_CORRUPT;
	}

	PmxGlobals globals;
	globals.text_encoding = reader.u8();
	globals.extra_uv = reader.u8();
	globals.vertex_index_size = reader.u8();
	globals.texture_index_size = reader.u8();
	globals.material_index_size = reader.u8();
	globals.bone_index_size = reader.u8();
	globals.morph_index_size = reader.u8();
	globals.rigid_body_index_size = reader.u8();
	for (int32_t i = 8; i < globals_count; i++) {
		reader.u8();
	}

	const bool utf8 = globals.text_encoding != 0;

	r_model.name = reader.string_utf(utf8);
	r_model.name_en = reader.string_utf(utf8);
	reader.string_utf(utf8); // comment
	reader.string_utf(utf8); // comment (english)

	// --- Vertices -----------------------------------------------------------
	const int32_t vertex_count = reader.i32();
	if (vertex_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.vertices.resize(static_cast<size_t>(vertex_count));
	for (int32_t i = 0; i < vertex_count && reader.ok(); i++) {
		PmxVertex &vertex = r_model.vertices[static_cast<size_t>(i)];
		vertex.position = reader.v3_flip_z();
		vertex.normal = reader.v3_flip_z();
		vertex.uv = reader.v2();
		for (int32_t uv = 0; uv < globals.extra_uv; uv++) {
			reader.v4();
		}

		const int32_t weight_type = reader.u8();
		switch (weight_type) {
			case 0: { // BDEF1
				vertex.bones[0] = reader.index_signed(globals.bone_index_size);
				vertex.weights[0] = 1.0f;
			} break;
			case 1: { // BDEF2
				vertex.bones[0] = reader.index_signed(globals.bone_index_size);
				vertex.bones[1] = reader.index_signed(globals.bone_index_size);
				vertex.weights[0] = reader.f32();
				vertex.weights[1] = 1.0f - vertex.weights[0];
			} break;
			case 2: // BDEF4
			case 4: { // QDEF (treat as plain skinning)
				for (int32_t b = 0; b < 4; b++) {
					vertex.bones[b] = reader.index_signed(globals.bone_index_size);
				}
				for (int32_t b = 0; b < 4; b++) {
					vertex.weights[b] = reader.f32();
				}
			} break;
			case 3: { // SDEF, the spherical correction is ignored
				vertex.bones[0] = reader.index_signed(globals.bone_index_size);
				vertex.bones[1] = reader.index_signed(globals.bone_index_size);
				vertex.weights[0] = reader.f32();
				vertex.weights[1] = 1.0f - vertex.weights[0];
				reader.v3(); // C
				reader.v3(); // R0
				reader.v3(); // R1
			} break;
			default:
				return ERR_FILE_CORRUPT;
		}

		reader.f32(); // edge scale
	}

	// --- Faces --------------------------------------------------------------
	const int32_t index_count = reader.i32();
	if (index_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.indices.resize(index_count);
	for (int32_t i = 0; i < index_count && reader.ok(); i++) {
		r_model.indices[i] = reader.index_unsigned(globals.vertex_index_size);
	}

	// --- Textures -----------------------------------------------------------
	const int32_t texture_count = reader.i32();
	if (texture_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.textures.resize(static_cast<size_t>(texture_count));
	for (int32_t i = 0; i < texture_count && reader.ok(); i++) {
		r_model.textures[static_cast<size_t>(i)] = reader.string_utf(utf8);
	}

	// --- Materials ----------------------------------------------------------
	const int32_t material_count = reader.i32();
	if (material_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.materials.resize(static_cast<size_t>(material_count));
	int32_t surface_start = 0;
	for (int32_t i = 0; i < material_count && reader.ok(); i++) {
		PmxMaterial &material = r_model.materials[static_cast<size_t>(i)];
		material.name = reader.string_utf(utf8);
		reader.string_utf(utf8); // name (english)
		material.diffuse = Color(reader.f32(), reader.f32(), reader.f32(), reader.f32());
		reader.v3(); // specular colour
		material.specularity = reader.f32();
		material.ambient = Color(reader.f32(), reader.f32(), reader.f32(), 1.0f);

		const int32_t draw_flags = reader.u8();
		material.double_sided = (draw_flags & 0x01) != 0;

		reader.v4(); // edge colour
		reader.f32(); // edge scale
		material.texture_index = reader.index_signed(globals.texture_index_size);
		material.sphere_texture_index = reader.index_signed(globals.texture_index_size);
		material.sphere_mode = reader.u8();
		material.toon_shared = reader.u8() != 0;
		if (material.toon_shared) {
			material.toon_index = reader.u8();
		} else {
			material.toon_index = reader.index_signed(globals.texture_index_size);
		}
		reader.string_utf(utf8); // memo

		material.index_count = reader.i32();
		material.index_start = surface_start;
		surface_start += material.index_count;
	}

	// --- Bones --------------------------------------------------------------
	const int32_t bone_count = reader.i32();
	if (bone_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.bones.resize(static_cast<size_t>(bone_count));
	for (int32_t i = 0; i < bone_count && reader.ok(); i++) {
		PmxBone &bone = r_model.bones[static_cast<size_t>(i)];
		bone.name = reader.string_utf(utf8);
		reader.string_utf(utf8); // name (english)
		bone.position = reader.v3_flip_z();
		bone.parent = reader.index_signed(globals.bone_index_size);
		reader.i32(); // deform layer
		const uint16_t flags = reader.u16();

		if ((flags & 0x0001) != 0) {
			reader.index_signed(globals.bone_index_size); // tail bone
		} else {
			reader.v3_flip_z(); // tail offset
		}
		if ((flags & (0x0100 | 0x0200)) != 0) { // inherit rotation / translation
			reader.index_signed(globals.bone_index_size);
			reader.f32();
		}
		if ((flags & 0x0400) != 0) { // fixed axis
			reader.v3();
		}
		if ((flags & 0x0800) != 0) { // local axis
			reader.v3();
			reader.v3();
		}
		if ((flags & 0x2000) != 0) { // external parent
			reader.i32();
		}
		if ((flags & 0x0020) != 0) { // IK
			reader.index_signed(globals.bone_index_size); // target
			reader.i32(); // loop count
			reader.f32(); // angle limit
			const int32_t link_count = reader.i32();
			for (int32_t link = 0; link < link_count && reader.ok(); link++) {
				reader.index_signed(globals.bone_index_size);
				if (reader.u8() != 0) {
					reader.v3();
					reader.v3();
				}
			}
		}
	}

	// --- Morphs -------------------------------------------------------------
	const int32_t morph_count = reader.i32();
	if (morph_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.morphs.resize(static_cast<size_t>(morph_count));
	for (int32_t i = 0; i < morph_count && reader.ok(); i++) {
		PmxMorph &morph = r_model.morphs[static_cast<size_t>(i)];
		morph.name = reader.string_utf(utf8);
		reader.string_utf(utf8); // name (english)
		morph.panel = reader.u8();
		const int32_t morph_type = reader.u8();
		morph.type = morph_type;
		const int32_t offset_count = reader.i32();
		if (morph_type == 1 && offset_count > 0) {
			morph.vertex_offsets.reserve(static_cast<size_t>(offset_count));
		}
		for (int32_t m = 0; m < offset_count && reader.ok(); m++) {
			switch (morph_type) {
				case 0: // group
				case 9: { // flip (2.1)
					(void)reader.index_signed(globals.morph_index_size);
					(void)reader.f32();
				} break;
				case 1: { // vertex
					const int32_t vertex_index = reader.index_unsigned(globals.vertex_index_size);
					const Vector3 offset = reader.v3_flip_z();
					PmxVertexMorphOffset vertex_offset;
					vertex_offset.vertex = vertex_index;
					vertex_offset.offset = offset;
					morph.vertex_offsets.push_back(vertex_offset);
				} break;
				case 2: { // bone
					reader.index_signed(globals.bone_index_size);
					reader.v3_flip_z();
					reader.v4();
				} break;
				case 3: case 4: case 5: case 6: case 7: { // uv / extra uv
					reader.index_unsigned(globals.vertex_index_size);
					reader.v4();
				} break;
				case 8: { // material
					reader.index_signed(globals.material_index_size);
					reader.u8(); // calc mode
					reader.v4(); // diffuse
					reader.v3(); // specular
					reader.f32(); // specularity
					reader.v3(); // ambient
					reader.v4(); // edge colour
					reader.f32(); // edge size
					reader.v4(); // texture tint
					reader.v4(); // sphere tint
					reader.v4(); // toon tint
				} break;
				case 10: { // impulse (2.1)
					(void)reader.index_signed(globals.rigid_body_index_size);
					(void)reader.u8(); // local
					(void)reader.v3(); // velocity
					(void)reader.v3(); // torque
				} break;
				default:
					return ERR_FILE_CORRUPT;
			}
		}
	}

	// --- Display frames (parsed and dropped) -------------------------------
	const int32_t frame_count = reader.i32();
	if (frame_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	for (int32_t i = 0; i < frame_count && reader.ok(); i++) {
		reader.string_utf(utf8);
		reader.string_utf(utf8);
		reader.u8(); // special flag
		const int32_t element_count = reader.i32();
		for (int32_t e = 0; e < element_count && reader.ok(); e++) {
			const int32_t element_type = reader.u8();
			if (element_type == 0) {
				reader.index_signed(globals.bone_index_size);
			} else {
				reader.index_signed(globals.morph_index_size);
			}
		}
	}

	// --- Rigid bodies -------------------------------------------------------
	const int32_t body_count = reader.i32();
	if (body_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.rigid_bodies.resize(static_cast<size_t>(body_count));
	for (int32_t i = 0; i < body_count && reader.ok(); i++) {
		PmxRigidBody &body = r_model.rigid_bodies[static_cast<size_t>(i)];
		body.name = reader.string_utf(utf8);
		reader.string_utf(utf8); // name (english)
		body.bone = reader.index_signed(globals.bone_index_size);
		body.group = reader.u8();
		body.collision_mask = static_cast<int32_t>(reader.u16());
		body.shape = reader.u8();
		body.size = reader.v3();
		body.position = reader.v3_flip_z();
		body.rotation = reader.v3_flip_xy();
		body.mass = reader.f32();
		body.linear_damping = reader.f32();
		body.angular_damping = reader.f32();
		body.restitution = reader.f32();
		body.friction = reader.f32();
		body.mode = reader.u8();
	}

	// --- Joints -------------------------------------------------------------
	const int32_t joint_count = reader.i32();
	if (joint_count < 0) {
		return ERR_FILE_CORRUPT;
	}
	r_model.joints.resize(static_cast<size_t>(joint_count));
	for (int32_t i = 0; i < joint_count && reader.ok(); i++) {
		PmxJoint &joint = r_model.joints[static_cast<size_t>(i)];
		joint.name = reader.string_utf(utf8);
		reader.string_utf(utf8); // name (english)
		reader.u8(); // type (0 = 6DOF spring)
		joint.body_a = reader.index_signed(globals.rigid_body_index_size);
		joint.body_b = reader.index_signed(globals.rigid_body_index_size);
		joint.position = reader.v3_flip_z();
		joint.rotation = reader.v3_flip_xy();
		joint.linear_lower = reader.v3();
		joint.linear_upper = reader.v3();
		joint.angular_lower = reader.v3();
		joint.angular_upper = reader.v3();
		joint.linear_spring = reader.v3();
		joint.angular_spring = reader.v3();
	}

	return reader.ok() ? OK : ERR_FILE_CORRUPT;
}
