#include "ofbx.h"

#include "imgui.h"
#include "imgui-framework.h"

// framework includes
#include "framework.h"

// libgg includes
#include "StringEx.h"

// system includes
#include <cassert>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <memory>
#include <vector>

#ifndef MAX_PATH
	#define MAX_PATH PATH_MAX
#endif

typedef char Path[MAX_PATH];
typedef unsigned int u32;
ofbx::IScene* g_scene = nullptr;
const ofbx::IElement* g_selected_element = nullptr;
const ofbx::Object* g_selected_object = nullptr;


template <int N>
void toString(ofbx::DataView view, char (&out)[N])
{
	int len = int(view.end - view.begin);
	if (len > sizeof(out) - 1) len = sizeof(out) - 1;
	strncpy(out, (const char*)view.begin, len);
	out[len] = 0;
}


int getPropertyCount(ofbx::IElementProperty* prop)
{
	return prop ? getPropertyCount(prop->getNext()) + 1 : 0;
}


template <int N>
void catProperty(char(&out)[N], const ofbx::IElementProperty& prop)
{
	char tmp[128];
	switch (prop.getType())
	{
		case ofbx::IElementProperty::DOUBLE: sprintf_s(tmp, sizeof(tmp), "%f", prop.getValue().toDouble()); break;
		case ofbx::IElementProperty::LONG: sprintf_s(tmp, sizeof(tmp), "%" PRId64, prop.getValue().toU64()); break;
		case ofbx::IElementProperty::INTEGER: sprintf_s(tmp, sizeof(tmp), "%d", prop.getValue().toInt()); break;
		case ofbx::IElementProperty::STRING: prop.getValue().toString(tmp); break;
		default: sprintf_s(tmp, sizeof(tmp), "Type: %c", (char)prop.getType()); break;
	}
	strcat(out, tmp);
}


void showGUI(const ofbx::IElement& parent)
{
	for (const ofbx::IElement* element = parent.getFirstChild(); element; element = element->getSibling())
	{
		auto id = element->getID();
		char label[128];
		id.toString(label);
		strcat(label, " (");
		ofbx::IElementProperty* prop = element->getFirstProperty();
		bool first = true;
		while (prop)
		{
			if (!first)
				strcat(label, ", ");
			first = false;
			catProperty(label, *prop);
			prop = prop->getNext();
		}
		strcat(label, ")");

		ImGui::PushID((const void*)id.begin);
		ImGuiTreeNodeFlags flags = g_selected_element == element ? ImGuiTreeNodeFlags_Selected : 0;
		if (!element->getFirstChild()) flags |= ImGuiTreeNodeFlags_Leaf;
		if (ImGui::TreeNodeEx(label, flags))
		{
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_element = element;
			if (element->getFirstChild()) showGUI(*element);
			ImGui::TreePop();
		}
		else
		{
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_element = element;
		}
		ImGui::PopID();
	}
}


template <typename T>
void showArray(const char* label, const char* format, ofbx::IElementProperty& prop)
{
	if (!ImGui::CollapsingHeader(label)) return;

	int count = prop.getCount();
	ImGui::Text("Count: %d", count);
	std::vector<T> tmp;
	tmp.resize(count);
	prop.getValues(&tmp[0], int(sizeof(tmp[0]) * tmp.size()));
	for (T v : tmp)
	{
		ImGui::Text(format, v);
	}
}


void showGUI(ofbx::IElementProperty& prop)
{
	ImGui::PushID((void*)&prop);
	char tmp[256];
	switch (prop.getType())
	{
		case ofbx::IElementProperty::LONG: ImGui::Text("Long: %" PRIu64, prop.getValue().toU64()); break;
		case ofbx::IElementProperty::FLOAT: ImGui::Text("Float: %f", prop.getValue().toFloat()); break;
		case ofbx::IElementProperty::DOUBLE: ImGui::Text("Double: %f", prop.getValue().toDouble()); break;
		case ofbx::IElementProperty::INTEGER: ImGui::Text("Integer: %d", prop.getValue().toInt()); break;
		case ofbx::IElementProperty::ARRAY_FLOAT: showArray<float>("float array", "%f", prop); break;
		case ofbx::IElementProperty::ARRAY_DOUBLE: showArray<double>("double array", "%f", prop); break;
		case ofbx::IElementProperty::ARRAY_INT: showArray<int>("int array", "%d", prop); break;
		case ofbx::IElementProperty::ARRAY_LONG: showArray<ofbx::u64>("long array", "%" PRId64, prop); break;
		case ofbx::IElementProperty::STRING:
			toString(prop.getValue(), tmp);
			ImGui::Text("String: %s", tmp);
			break;
		default:
			ImGui::Text("Other: %c", (char)prop.getType());
			break;
	}

	ImGui::PopID();
	if (prop.getNext()) showGUI(*prop.getNext());
}


static void showCurveGUI(const ofbx::Object& object) {
    const ofbx::AnimationCurve& curve = static_cast<const ofbx::AnimationCurve&>(object);
    
    const int c = curve.getKeyCount();
    for (int i = 0; i < c; ++i) {
        const float t = (float)ofbx::fbxTimeToSeconds(curve.getKeyTime()[i]);
        const float v = curve.getKeyValue()[i];
        ImGui::Text("%fs: %f ", t, v);
        
    }
}


void showObjectGUI(const ofbx::Object& object)
{
	const char* label;
	switch (object.getType())
	{
		case ofbx::Object::Type::GEOMETRY: label = "geometry"; break;
		case ofbx::Object::Type::MESH: label = "mesh"; break;
		case ofbx::Object::Type::MATERIAL: label = "material"; break;
		case ofbx::Object::Type::ROOT: label = "root"; break;
		case ofbx::Object::Type::TEXTURE: label = "texture"; break;
		case ofbx::Object::Type::NULL_NODE: label = "null"; break;
		case ofbx::Object::Type::LIMB_NODE: label = "limb node"; break;
		case ofbx::Object::Type::NODE_ATTRIBUTE: label = "node attribute"; break;
		case ofbx::Object::Type::CLUSTER: label = "cluster"; break;
		case ofbx::Object::Type::SKIN: label = "skin"; break;
		case ofbx::Object::Type::ANIMATION_STACK: label = "animation stack"; break;
		case ofbx::Object::Type::ANIMATION_LAYER: label = "animation layer"; break;
		case ofbx::Object::Type::ANIMATION_CURVE: label = "animation curve"; break;
		case ofbx::Object::Type::ANIMATION_CURVE_NODE: label = "animation curve node"; break;
		default: assert(false); break;
	}

	ImGuiTreeNodeFlags flags = g_selected_object == &object ? ImGuiTreeNodeFlags_Selected : 0;
	char tmp[128];
	sprintf_s(tmp, sizeof(tmp), "%" PRIu64 " %s (%s)", object.id, object.name, label);
	if (ImGui::TreeNodeEx(tmp, flags))
	{
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_object = &object;
		int i = 0;
		while (ofbx::Object* child = object.resolveObjectLink(i))
		{
			showObjectGUI(*child);
			++i;
		}
        if(object.getType() == ofbx::Object::Type::ANIMATION_CURVE) {
            showCurveGUI(object);
        }

		ImGui::TreePop();
	}
	else
	{
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_object = &object;
	}
}


void showObjectsGUI(const ofbx::IScene& scene)
{
	if (!ImGui::Begin("Objects"))
	{
		ImGui::End();
		return;
	}
	const ofbx::Object* root = scene.getRoot();
	if (root) showObjectGUI(*root);

	int count = scene.getAnimationStackCount();
	for (int i = 0; i < count; ++i)
	{
		const ofbx::Object* stack = scene.getAnimationStack(i);
		showObjectGUI(*stack);
	}

	ImGui::End();
}


bool saveAsOBJ(ofbx::IScene& scene, const char* path)
{
	FILE* fp = fopen(path, "wb");
	if (!fp) return false;
	int obj_idx = 0;
	int indices_offset = 0;
	int normals_offset = 0;
	int mesh_count = scene.getMeshCount();
	for (int i = 0; i < mesh_count; ++i)
	{
		fprintf(fp, "o obj%d\ng grp%d\n", i, obj_idx);

		const ofbx::Mesh& mesh = *scene.getMesh(i);
		const ofbx::Geometry& geom = *mesh.getGeometry();
		int vertex_count = geom.getVertexCount();
		const ofbx::Vec3* vertices = geom.getVertices();
		for (int i = 0; i < vertex_count; ++i)
		{
			ofbx::Vec3 v = vertices[i];
			fprintf(fp, "v %f %f %f\n", v.x, v.y, v.z);
		}

		bool has_normals = geom.getNormals() != nullptr;
		if (has_normals)
		{
			const ofbx::Vec3* normals = geom.getNormals();
			int count = geom.getIndexCount();

			for (int i = 0; i < count; ++i)
			{
				ofbx::Vec3 n = normals[i];
				fprintf(fp, "vn %f %f %f\n", n.x, n.y, n.z);
			}
		}

		bool has_uvs = geom.getUVs() != nullptr;
		if (has_uvs)
		{
			const ofbx::Vec2* uvs = geom.getUVs();
			int count = geom.getIndexCount();

			for (int i = 0; i < count; ++i)
			{
				ofbx::Vec2 uv = uvs[i];
				fprintf(fp, "vt %f %f\n", uv.x, uv.y);
			}
		}

		const int* faceIndices = geom.getFaceIndices();
		int index_count = geom.getIndexCount();
		bool new_face = true;
		for (int i = 0; i < index_count; ++i)
		{
			if (new_face)
			{
				fputs("f ", fp);
				new_face = false;
			}
			int idx = (faceIndices[i] < 0) ? -faceIndices[i] : (faceIndices[i] + 1);
			int vertex_idx = indices_offset + idx;
			fprintf(fp, "%d", vertex_idx);

			if (has_uvs)
			{
				int uv_idx = normals_offset + i + 1;
				fprintf(fp, "/%d", uv_idx);
			}
			else
			{
				fprintf(fp, "/");
			}

			if (has_normals)
			{
				int normal_idx = normals_offset + i + 1;
				fprintf(fp, "/%d", normal_idx);
			}
			else
			{
				fprintf(fp, "/");
			}

			new_face = faceIndices[i] < 0;
			fputc(new_face ? '\n' : ' ', fp);
		}

		indices_offset += vertex_count;
		normals_offset += index_count;
		++obj_idx;
	}
	fclose(fp);
	return true;
}


void onGUI()
{
	if (g_scene)
	{
//		ImGui::RootDock(ImVec2(0, 0), ImGui::GetIO().DisplaySize);
		if (ImGui::Begin("Elements"))
		{
			const ofbx::IElement* root = g_scene->getRootElement();
			if (root && root->getFirstChild()) showGUI(*root);
		}
		ImGui::End();

		if (ImGui::Begin("Properties") && g_selected_element)
		{
			ofbx::IElementProperty* prop = g_selected_element->getFirstProperty();
			if (prop) showGUI(*prop);
		}
		ImGui::End();

		showObjectsGUI(*g_scene);
	}
}

bool init()
{
	FILE* fp = fopen("a.fbx", "rb");
	if (!fp) return false;

	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	auto* content = new ofbx::u8[file_size];
	fread(content, 1, file_size, fp);
	g_scene = ofbx::load((ofbx::u8*)content, file_size, (ofbx::u64)ofbx::LoadFlags::TRIANGULATE);
	if(!g_scene) {
        logError(ofbx::getError());
    }
    else {
        saveAsOBJ(*g_scene, "out.obj");
    }
	delete[] content;
	fclose(fp);

	return true;
}

int main(int argc, char * argv[])
{
	setupPaths(CHIBI_RESOURCE_PATHS);
	
	if (!framework.init(800, 600))
		return -1;
	
	FrameworkImGuiContext guiContext;
	guiContext.init();
	
	init();

	while (!framework.quitRequested)
	{
		framework.process();
		
		bool inputIsCaptured = false;
		guiContext.processBegin(framework.timeStep, framework.getCurrentWindow().getWidth(), framework.getCurrentWindow().getHeight(), inputIsCaptured);
		{
			onGUI();
		}
		guiContext.processEnd();
		
		framework.beginDraw(0, 0, 0, 0);
		{
			guiContext.draw();
		}
		framework.endDraw();
	}

	guiContext.shut();
	
	framework.shutdown();

	return 0;
}
