#include "stkmesh.hpp"
#include "graphics/irr_driver.hpp"
#include <ISceneManager.h>
#include <IMaterialRenderer.h>
#include "config/user_config.hpp"

static
GLuint createVAO(GLuint vbo, GLuint idx, GLuint attrib_position, GLuint attrib_texcoord, GLuint attrib_normal, size_t stride)
{
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(attrib_position);
	if ((GLint)attrib_texcoord != -1)
		glEnableVertexAttribArray(attrib_texcoord);
	if ((GLint)attrib_normal != -1)
		glEnableVertexAttribArray(attrib_normal);
	glVertexAttribPointer(attrib_position, 3, GL_FLOAT, GL_FALSE, stride, 0);
	if ((GLint)attrib_texcoord != -1)
		glVertexAttribPointer(attrib_texcoord, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*) 28);
	if ((GLint)attrib_normal != -1)
		glVertexAttribPointer(attrib_normal, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*) 12);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
	glBindVertexArray(0);
	return vao;
}

namespace ObjectPass1Shader
{
	GLuint Program;
	GLuint attrib_position, attrib_normal;
	GLuint uniform_MVP, uniform_TIMV;

	void init()
	{
		initGL();
		Program = LoadProgram(file_manager->getAsset("shaders/object_pass1.vert").c_str(), file_manager->getAsset("shaders/object_pass1.frag").c_str());
		attrib_position = glGetAttribLocation(Program, "Position");
		attrib_normal = glGetAttribLocation(Program, "Normal");
		uniform_MVP = glGetUniformLocation(Program, "ModelViewProjectionMatrix");
		uniform_TIMV = glGetUniformLocation(Program, "TransposeInverseModelView");;
	}

	void setUniforms(const core::matrix4 &ModelViewProjectionMatrix, const core::matrix4 &TransposeInverseModelView)
	{
		glUniformMatrix4fv(uniform_MVP, 1, GL_FALSE, ModelViewProjectionMatrix.pointer());
		glUniformMatrix4fv(uniform_TIMV, 1, GL_FALSE, TransposeInverseModelView.pointer());
	}
}

namespace ObjectPass2Shader
{
	GLuint Program;
	GLuint attrib_position, attrib_texcoord;
	GLuint uniform_MVP, uniform_TIMV, uniform_Albedo, uniform_DiffuseMap, uniform_SpecularMap, uniform_SSAO, uniform_screen, uniform_ambient;

	void init()
	{
		initGL();
		Program = LoadProgram(file_manager->getAsset("shaders/object_pass2.vert").c_str(), file_manager->getAsset("shaders/object_pass2.frag").c_str());
		attrib_position = glGetAttribLocation(Program, "Position");
		attrib_texcoord = glGetAttribLocation(Program, "Texcoord");
		uniform_MVP = glGetUniformLocation(Program, "ModelViewProjectionMatrix");
		uniform_Albedo = glGetUniformLocation(Program, "Albedo");
		uniform_DiffuseMap = glGetUniformLocation(Program, "DiffuseMap");
		uniform_SpecularMap = glGetUniformLocation(Program, "SpecularMap");
		uniform_SSAO = glGetUniformLocation(Program, "SSAO");
		uniform_screen = glGetUniformLocation(Program, "screen");
		uniform_ambient = glGetUniformLocation(Program, "ambient");
	}

	void setUniforms(const core::matrix4 &ModelViewProjectionMatrix, unsigned TU_Albedo, unsigned TU_DiffuseMap, unsigned TU_SpecularMap, unsigned TU_SSAO)
	{
		glUniformMatrix4fv(uniform_MVP, 1, GL_FALSE, ModelViewProjectionMatrix.pointer());
		glUniform1i(uniform_Albedo, TU_Albedo);
		glUniform1i(uniform_DiffuseMap, TU_DiffuseMap);
		glUniform1i(uniform_SpecularMap, TU_SpecularMap);
		glUniform1i(uniform_SSAO, TU_SSAO);
		glUniform2f(uniform_screen, UserConfigParams::m_width, UserConfigParams::m_height);
		const video::SColorf s = irr_driver->getSceneManager()->getAmbientLight();
		glUniform3f(uniform_ambient, s.r, s.g, s.b);
	}
}

static
GLMesh allocateMeshBuffer(scene::IMeshBuffer* mb)
{
	initGL();
	GLMesh result = {};
	if (!mb)
		return result;
	glGenBuffers(1, &(result.vertex_buffer));
	glGenBuffers(1, &(result.index_buffer));

	glBindBuffer(GL_ARRAY_BUFFER, result.vertex_buffer);
	const void* vertices = mb->getVertices();
	const u32 vertexCount = mb->getVertexCount();
	const irr::video::E_VERTEX_TYPE vType = mb->getVertexType();
	result.Stride = getVertexPitchFromType(vType);
	const c8* vbuf = static_cast<const c8*>(vertices);
	glBufferData(GL_ARRAY_BUFFER, vertexCount * result.Stride, vbuf, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result.index_buffer);
	const void* indices = mb->getIndices();
	u32 indexCount = mb->getIndexCount();
	GLenum indexSize;
	switch (mb->getIndexType())
	{
		case irr::video::EIT_16BIT:
		{
			indexSize = sizeof(u16);
			result.IndexType = GL_UNSIGNED_SHORT;
			break;
		}
		case irr::video::EIT_32BIT:
		{
			indexSize = sizeof(u32);
			result.IndexType = GL_UNSIGNED_INT;
			break;
		}
		default:
		{
			assert(0 && "Wrong index size");
		}
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * indexSize, indices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	result.IndexCount = mb->getIndexCount();
	switch (mb->getPrimitiveType())
	{
	case scene::EPT_POINTS:
		result.PrimitiveType = GL_POINTS;
		break;
	case scene::EPT_TRIANGLE_STRIP:
		result.PrimitiveType = GL_TRIANGLE_STRIP;
		break;
	case scene::EPT_TRIANGLE_FAN:
		result.PrimitiveType = GL_TRIANGLE_FAN;
		break;
	case scene::EPT_LINES:
		result.PrimitiveType = GL_LINES;
	case scene::EPT_TRIANGLES:
		result.PrimitiveType = GL_TRIANGLES;
		break;
	case scene::EPT_POINT_SPRITES:
	case scene::EPT_LINE_LOOP:
	case scene::EPT_POLYGON:
	case scene::EPT_LINE_STRIP:
	case scene::EPT_QUAD_STRIP:
	case scene::EPT_QUADS:
		assert(0 && "Unsupported primitive type");
	}
	ITexture *tex = mb->getMaterial().getTexture(0);
	if (tex)
		result.textures = static_cast<irr::video::COpenGLTexture*>(tex)->getOpenGLTextureName();
	else
		result.textures = 0;
	return result;
}

STKMesh::STKMesh(irr::scene::IMesh* mesh, ISceneNode* parent, irr::scene::ISceneManager* mgr,	irr::s32 id,
	const irr::core::vector3df& position,
	const irr::core::vector3df& rotation,
	const irr::core::vector3df& scale) :
		CMeshSceneNode(mesh, parent, mgr, id, position, rotation, scale)
{
	for (u32 i=0; i<Mesh->getMeshBufferCount(); ++i)
	{
		scene::IMeshBuffer* mb = Mesh->getMeshBuffer(i);
		GLmeshes.push_back(allocateMeshBuffer(mb));

	}
	if (ObjectPass1Shader::Program && ObjectPass2Shader::Program)
		return;
	ObjectPass1Shader::init();
	ObjectPass2Shader::init();
}

STKMesh::~STKMesh()
{
//	glDeleteBuffers(vertex_buffer.size(), vertex_buffer.data());
//	glDeleteBuffers(index_buffer.size(), index_buffer.data());
}

static
void drawFirstPass(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
  irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getRTT(RTT_NORMAL_AND_DEPTH), false, false);

  glStencilFunc(GL_ALWAYS, 0, ~0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_ALPHA_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  GLenum ptype = mesh.PrimitiveType;
  GLenum itype = mesh.IndexType;
  size_t count = mesh.IndexCount;

  core::matrix4 ModelViewProjectionMatrix = irr_driver->getVideoDriver()->getTransform(video::ETS_PROJECTION);
  ModelViewProjectionMatrix *= irr_driver->getVideoDriver()->getTransform(video::ETS_VIEW);
  ModelViewProjectionMatrix *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);
  core::matrix4 TransposeInverseModelView = irr_driver->getVideoDriver()->getTransform(video::ETS_VIEW);
  TransposeInverseModelView *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);
  TransposeInverseModelView.makeInverse();
  TransposeInverseModelView = TransposeInverseModelView.getTransposed();

  glUseProgram(ObjectPass1Shader::Program);
  ObjectPass1Shader::setUniforms(ModelViewProjectionMatrix, TransposeInverseModelView);

  glBindVertexArray(mesh.vao_first_pass);
  glDrawElements(ptype, count, itype, 0);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glStencilFunc(GL_ALWAYS, 1, ~0);
  irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getMainSetup(), false, false);
}

static
void drawSecondPass(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
  irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getRTT(RTT_COLOR), false, false);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_ALPHA_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  GLenum ptype = mesh.PrimitiveType;
  GLenum itype = mesh.IndexType;
  size_t count = mesh.IndexCount;

  core::matrix4 ModelViewProjectionMatrix = irr_driver->getVideoDriver()->getTransform(video::ETS_PROJECTION);
  ModelViewProjectionMatrix *= irr_driver->getVideoDriver()->getTransform(video::ETS_VIEW);
  ModelViewProjectionMatrix *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, mesh.textures);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, static_cast<irr::video::COpenGLTexture*>(irr_driver->getRTT(RTT_TMP1))->getOpenGLTextureName());
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, static_cast<irr::video::COpenGLTexture*>(irr_driver->getRTT(RTT_TMP2))->getOpenGLTextureName());
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, static_cast<irr::video::COpenGLTexture*>(irr_driver->getRTT(RTT_SSAO))->getOpenGLTextureName());

  glUseProgram(ObjectPass2Shader::Program);
  ObjectPass2Shader::setUniforms(ModelViewProjectionMatrix, 0, 1, 2, 3);

  glBindVertexArray(mesh.vao_second_pass);
  glDrawElements(ptype, count, itype, 0);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getRTT(RTT_COLOR), false, false);
}

static
void draw(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
	if (!mesh.textures)
		return;
	if (irr_driver->getPhase() == 0)
		drawFirstPass(mesh, type);
	else
		drawSecondPass(mesh, type);

	video::SMaterial material;
	material.MaterialType = irr_driver->getShader(ES_RAIN);
	material.BlendOperation = video::EBO_NONE;
	material.ZWriteEnable = true;
	material.Lighting = false;
	irr_driver->getVideoDriver()->setMaterial(material);
	static_cast<irr::video::COpenGLDriver*>(irr_driver->getVideoDriver())->setRenderStates3DMode();
}

static bool isObject(video::E_MATERIAL_TYPE type)
{
	if (type == irr_driver->getShader(ES_OBJECTPASS))
		return true;
	return false;
}

static void initvaostate(GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
	if (mesh.vao_first_pass)
		return;

	mesh.vao_first_pass = createVAO(mesh.vertex_buffer, mesh.index_buffer,
		ObjectPass1Shader::attrib_position, -1, ObjectPass1Shader::attrib_normal,
		mesh.Stride);
	mesh.vao_second_pass = createVAO(mesh.vertex_buffer, mesh.index_buffer,
		ObjectPass2Shader::attrib_position, ObjectPass2Shader::attrib_texcoord, -1,
		mesh.Stride);
}

void STKMesh::render()
{
	irr::video::IVideoDriver* driver = irr_driver->getVideoDriver();

	if (!Mesh || !driver)
		return;

	bool isTransparentPass =
		SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	Box = Mesh->getBoundingBox();

	for (u32 i=0; i<Mesh->getMeshBufferCount(); ++i)
	{
		scene::IMeshBuffer* mb = Mesh->getMeshBuffer(i);
		if (mb)
		{
			const video::SMaterial& material = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];

			video::IMaterialRenderer* rnd = driver->getMaterialRenderer(material.MaterialType);
			bool transparent = (rnd && rnd->isTransparent());

			// only render transparent buffer if this is the transparent render pass
			// and solid only in solid pass
			if (isObject(material.MaterialType) && !isTransparentPass && !transparent)
			{
				initvaostate(GLmeshes[i], material.MaterialType);
				draw(GLmeshes[i], material.MaterialType);
			}
			else if (transparent == isTransparentPass)
			{
				driver->setMaterial(material);
				driver->drawMeshBuffer(mb);
			}
		}
	}
}
