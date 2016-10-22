#include "scene.h"
#include "device.h"

#include <algorithm>

Material::Handle Material::DEFAULT_MATERIAL = -1;

namespace Object
{
	void Desc::Identity()
	{
		modelMatrix.Identity();
		position = vec3f( 0 );
		rotation = vec3f( 0 );
		scale = 1.f;
	}

	void Desc::Translate( const vec3f &t )
	{
		position += t;
	}

	void Desc::Scale( const vec3f &s )
	{
		scale *= s;
	}

	void Desc::Rotate( const vec3f &r )
	{
		rotation += r;
	}

	void Desc::ApplyTransform()
	{
		modelMatrix.FromTRS( position, rotation, scale );
	}
}

namespace Text
{
	void Desc::SetPosition( const vec2f &pos )
	{
		model_matrix = mat4f::Translation( vec3f( pos.x, pos.y, 0.f ) );
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//				AREA LIGHT
//////////////////////////////////////////////////////////////////////////////////////////////////

void InitRectPoints( AreaLight::UniformBufferData &rect, vec3f points[4] )
{
	vec3f ex = rect.dirx * rect.hwidthx;
	vec3f ey = rect.diry * rect.hwidthy;

	// LogInfo(rect.diry.x, " ", rect.diry.y, " ", rect.diry.z, " | ", ey.x, " ", ey.y);

	points[0] = rect.position - ex - ey;
	points[1] = rect.position + ex - ey;
	points[2] = rect.position + ex + ey;
	points[3] = rect.position - ex + ey;
}

Rectangle AreaLight::GetRectangle( const UniformBufferData & al )
{
	Rectangle r;
	r.position = al.position;
	r.ex = al.dirx;
	r.ey = al.diry;
	r.ez = vec3f( al.plane.x, al.plane.y, al.plane.z );
	r.hx = al.hwidthx;
	r.hy = al.hwidthy;

	// store the rectangle's vertices in p0, p1, p2, p3
	GetVertices( al, &r.p0 );

	return r;
}

void AreaLight::GetVertices( const AreaLight::UniformBufferData &rect, vec3f points[4] )
{
	vec3f ex = rect.dirx * rect.hwidthx;
	vec3f ey = rect.diry * rect.hwidthy;

	points[0] = rect.position - ex - ey;
	points[1] = rect.position + ex - ey;
	points[2] = rect.position + ex + ey;
	points[3] = rect.position - ex + ey;
}

bool AreaLight::Cull( const AreaLight::UniformBufferData &al, const vec3f &P, const vec3f &N )
{
	vec3f points[4];
	GetVertices( al, points );

	const vec3f pN( al.plane.x, al.plane.y, al.plane.z );
	const f32 w = -Dot( P, N );

	bool pRightSide = Dot( P, pN ) + al.plane.w > 1e-5f;

	bool aRightSide =
		( Dot( N, points[0] ) + w > 1e-5f ) ||
		( Dot( N, points[1] ) + w > 1e-5f ) ||
		( Dot( N, points[2] ) + w > 1e-5f ) ||
		( Dot( N, points[3] ) + w > 1e-5f );

	return !( pRightSide && aRightSide );
}

void SceneResizeEventListener( const Event &event, void *data )
{
	Scene *scene = static_cast<Scene*>( data );
	// scene->UpdateProjection(event.v);
}

Scene::Scene() 
{
	Clean();
}

bool Scene::Init()
{	
	pickedObject = -1;
	pickedTriangle = -1;
	
	texts.reserve( 256 );
	objects.reserve( 1024 );
	materials.reserve( 64 );
	pointLights.reserve( 32 );
	skyboxes.reserve( 16 );

	for ( u32 i = 0; i < SCENE_MAX_ACTIVE_LIGHTS; ++i )
	{
		active_pointLights[i] = -1;
	}

	// Point Lights
	Render::UBO::Desc ubo_desc( NULL, SCENE_MAX_ACTIVE_LIGHTS * sizeof( PointLight::UniformBufferData ), Render::UBO::ST_DYNAMIC );
	pointLightsUBO = Render::UBO::Build( ubo_desc );
	if ( pointLightsUBO < 0 )
	{
		LogErr( "Error creating point light's UBO." );
		return false;
	}
	

	Render::Font::Desc fdesc( "../radar/data/DejaVuSans.ttf", 12 );
	Render::Font::Handle fhandle = Render::Font::Build( fdesc );
	if ( fhandle < 0 )
	{
		LogErr( "Error loading DejaVuSans font." );
		return false;
	}

	Material::Desc mat_desc;
	Material::DEFAULT_MATERIAL = Add( mat_desc );
	if ( Material::DEFAULT_MATERIAL < 0 )
	{
		LogErr( "Error adding Default Material" );
		return false;
	}


	// Default Skybox (white)
	skyboxMesh = Render::Mesh::BuildBox();
	if ( skyboxMesh < 0 )
	{
		LogErr( "Error creating skybox mesh." );
		return false;
	}

	Skybox::Desc sd;
	sd.filenames[0] = "../radar/data/default_diff.png";
	sd.filenames[1] = "../radar/data/default_diff.png";
	sd.filenames[2] = "../radar/data/default_diff.png";
	sd.filenames[3] = "../radar/data/default_diff.png";
	sd.filenames[4] = "../radar/data/default_diff.png";
	sd.filenames[5] = "../radar/data/default_diff.png";

	Skybox::Handle sh = Add( sd );
	if ( sh < 0 )
	{
		LogErr( "Error creating default white skybox." );
		return false;
	}
	SetSkybox( sh );

	return true;
}

void Scene::Clean()
{
	objects.clear();
	texts.clear();
	materials.clear();
	skyboxes.clear();
}






bool Scene::MaterialExists( Material::Handle h ) const
{
	return h >= 0 && h < (int) materials.size() && Render::UBO::Exists( materials[h].ubo );
}

void Scene::SetTextString( Text::Handle h, const std::string &str )
{
	if ( h >= 0 && h < (int) texts.size() )
	{
		Text::Desc &text = texts[h];
		text.str = str;
		text.mesh = Render::TextMesh::SetString( text.mesh, text.font, text.str );
	}
}

Object::Handle Scene::Add( const Object::Desc &d )
{
	if ( !Render::Shader::Exists( d.shader ) )
	{
		LogErr( "Given shader is not registered in renderer." );
		return -1;
	}

	// Check submesh existence
	for ( u32 i = 0; i < d.numSubmeshes; ++i )
	{
		if ( !Render::Mesh::Exists( d.meshes[i] ) )
		{
			LogErr( "Submesh ", i, " is not registered in renderer." );
			return -1;
		}
		if ( !MaterialExists( d.materials[i] ) )
		{
			LogErr( "Material ", i, " is not registered in the scene." );
			return -1;
		}
	}

	size_t index = objects.size();

	objects.push_back( d );

	return ( Object::Handle )index;
}

PointLight::Handle Scene::Add( const PointLight::Desc &d )
{
	size_t index = pointLights.size();
	
	pointLights.push_back( d );

	PointLight::Desc &light = pointLights[index];
	light.active = true;
	// look for active slot
	for ( u32 i = 0; i < SCENE_MAX_ACTIVE_LIGHTS; ++i )
	{
		if ( active_pointLights[i] < 0 )
		{
			active_pointLights[i] = (int) index;
			break;
		}
	}

	return ( PointLight::Handle )index;
}

Material::Data *Scene::GetMaterial( Material::Handle h )
{
	if ( MaterialExists( h ) )
		return &materials[h];
	return nullptr;
}

Text::Handle Scene::Add( const Text::Desc &d )
{
	if ( !Render::Font::Exists( d.font ) )
	{
		LogErr( "Given font is not registered in renderer." );
		return -1;
	}

	size_t index = texts.size();
	
	texts.push_back( d );

	Text::Desc &text = texts[index];
	text.mesh = -1;

	SetTextString( ( Text::Handle )index, text.str );

	return ( Text::Handle )index;
}


void Material::Data::ReloadUBO()
{
	if ( ubo >= 0 )
	{
		Render::UBO::Desc ubo_desc( (f32*) &desc.uniform, sizeof( Material::Desc::UniformBufferData ), Render::UBO::ST_DYNAMIC );
		Render::UBO::Update( ubo, ubo_desc );
	}
}

Material::Handle Scene::Add( const Material::Desc &d )
{
	size_t idx = materials.size();

	Material::Data mat;
	mat.desc = d;

	// Create UBO to accomodate it on GPU
	Render::UBO::Desc ubo_desc( (f32*) &d.uniform, sizeof( Material::Desc::UniformBufferData ), d.dynamic ? Render::UBO::ST_DYNAMIC : Render::UBO::ST_STATIC );
	mat.ubo = Render::UBO::Build( ubo_desc );
	if ( mat.ubo < 0 )
	{
		LogErr( "Error creating Material" );
		return -1;
	}

	// Load textures if present
	if ( d.diffuseTexPath != "" )
	{	// Diffuse
		Render::Texture::Desc t_desc( d.diffuseTexPath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading diffuse texture ", d.diffuseTexPath );
			return -1;
		}
		mat.diffuseTex = t_h;
	}
	else
	{
		// Default diffuse texture
		mat.diffuseTex = Render::Texture::DEFAULT_DIFFUSE;
	}

	if ( d.specularTexPath != "" )
	{
		Render::Texture::Desc t_desc( d.specularTexPath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading specular texture ", d.specularTexPath );
			return -1;
		}
		// LogDebug("Loaded specular texture at idx ", t_h);
		mat.specularTex = t_h;
	}
	else
	{
		mat.specularTex = Render::Texture::DEFAULT_DIFFUSE;	 // 1 specular, multiplied by the mat.shininess
	}

	if ( d.normalTexPath != "" )
	{
		Render::Texture::Desc t_desc( d.normalTexPath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading normal texture ", d.normalTexPath );
			return -1;
		}
		mat.normalTex = t_h;
	}
	else
	{
		mat.normalTex = Render::Texture::DEFAULT_NORMAL;
	}

	if ( d.occlusionTexPath != "" )
	{
		Render::Texture::Desc t_desc( d.occlusionTexPath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading occlusion texture ", d.occlusionTexPath );
			return -1;
		}
		mat.occlusionTex = t_h;
	}
	else
	{
		mat.occlusionTex = Render::Texture::DEFAULT_DIFFUSE;
	}

	if ( d.ltcMatrixPath != "" )
	{
		Render::Texture::Desc t_desc( d.ltcMatrixPath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading ltcMatrix texture ", d.ltcMatrixPath );
			return -1;
		}
		mat.ltcMatrix = t_h;
	}
	else
	{
		mat.ltcMatrix = Render::Texture::DEFAULT_DIFFUSE;
	}

	if ( d.ltcAmplitudePath != "" )
	{
		Render::Texture::Desc t_desc( d.ltcAmplitudePath );
		Render::Texture::Handle t_h = Render::Texture::Build( t_desc );
		if ( t_h < 0 )
		{
			LogErr( "Error loading ltcAmplitude texture ", d.ltcAmplitudePath );
			return -1;
		}
		mat.ltcAmplitude = t_h;
	}
	else
	{
		mat.ltcAmplitude = Render::Texture::DEFAULT_DIFFUSE;
	}

	materials.push_back( mat );
	return ( Material::Handle ) idx;
}

Object::Desc *Scene::GetObject( Object::Handle h )
{
	if ( ObjectExists( h ) )
		return &objects[h];
	return nullptr;
}

bool Scene::ObjectExists( Object::Handle h )
{
	return h >= 0 && h < (int) objects.size();
}

Object::Handle Scene::InstanciateModel( const ModelResource::Handle &h, Render::Shader::Handle shader )
{
	size_t index = objects.size();
	ModelResource::Data &model = models[h];

	// Material::Desc mat_desc(col3f(0.181,0.1,0.01), col3f(.9,.5,.5), col3f(1,.8,0.2), 0.8);

	Object::Desc odesc( shader );//, model.subMeshes[i], model.materials[matIdx]);
	for ( u32 i = 0; i < model.numSubMeshes; ++i )
	{
		u32 matIdx = model.materialIdx[i];
		odesc.AddSubmesh( model.subMeshes[i], model.materials[matIdx] );
		// odesc.Translate(vec3f(-25,0,0));
		// odesc.Rotate(vec3f(0,-2.f*M_PI/2.3f,0));
		// odesc.Scale(vec3f(15,15,15));
		// odesc.model_matrix *= mat4f::Scale(10,10,10);
	}

	Object::Handle obj_h = Scene::Add( odesc );
	if ( obj_h < 0 )
	{
		LogErr( "Error creating Object from Model." );
		return -1;
	}
	return obj_h;
}

Skybox::Handle Scene::Add( const Skybox::Desc &d )
{
	size_t idx = skyboxes.size();

	Skybox::Data sky;
	Render::Texture::Desc td;
	td.type = Render::Texture::Cubemap;
	for ( u32 i = 0; i < 6; ++i )
	{
		td.name[i] = d.filenames[i];
	}
	sky.cubemap = Render::Texture::Build( td );
	if ( sky.cubemap < 0 )
	{
		LogErr( "Error creating Skybox." );
		return -1;
	}

	skyboxes.push_back( sky );
	return ( Skybox::Handle ) idx;
}

void Scene::SetSkybox( Skybox::Handle h )
{
	currSkybox = h;
	Render::Texture::BindCubemap( skyboxes[currSkybox].cubemap, Render::Texture::TARGET0 );
}