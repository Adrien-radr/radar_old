#include "SHIntegration.h"
#include "imgui/imgui.h"

#include "src/render_internal/geometry.h"
#include "src/common/sampling.h"

//#pragma optimize("", off)

AreaLight::Handle alh = -1;
AreaLight::Handle alh2;
AreaLight::Handle alh3;
vec3f alPos(40., 3, -20);
vec3f alPos2(20., 7.5, 20);

u32 nBand = 4;

// UI
static bool shNormalization = false;
static f32 GGXexponent = 0.5f;
static int method = 0;
static int brdfMethod = 1;
static int numSamples = 1024;
static bool autoUpdate = true;
static bool movement = true;
static bool doTests = false;

SHInt sh1;

#include "AxialMoments.hpp"
#include "Tests.h"


std::vector<f32> MonteCarloMoments(const Triangle& triangle, const vec3f& w, int n) {
	// Number of MC samples
	const int M = 100000;
	std::vector<f32> mean(n + 1, 0.f);

	for (int k = 0; k<M; ++k) {
		vec2f randV = Random::Vec2f();

		vec3f d;
		f32 invPdf = triangle.SampleDir(d, randV.x, randV.y) * triangle.area;

		//if (HitTriangle(triangle, d)) {
		for (int p = 0; p <= n; ++p) {
			const f32 val = std::pow(d.Dot(w), p) * 1.f * invPdf;
			mean[p] += val / f32(M);
		}
		//}
	}

	return mean;
}

int TestMoments(const vec3f &dir, const std::vector<vec3f> &v, int nMin, int nMax) {
	int errors = 0;

	std::vector<vec3f> verts(v);

	Rectangle rect(verts);

	for (u32 i = 0; i < verts.size(); ++i)
		verts[i].Normalize();

	Polygon P(verts);

	// Test with axial moments
	LogInfo("Computing Analytical Axial Moments.");
	std::vector<f32> moments(nMax + 1, 0.f);
	P.AxialMoment(dir, nMax, moments);

	// Test with Ground Truth
	LogInfo("Computing MC Ground Truth.");
	
	Triangle t1, t2;
	t1.InitUnit(P.edges[0].A, P.edges[1].A, P.edges[2].A, vec3f(0.f));
	t2.InitUnit(P.edges[0].A, P.edges[2].A, P.edges[3].A, vec3f(0.f));
	std::vector<f32> mcMoments = MonteCarloMoments(t1, dir, nMax);
	{
		std::vector<f32> tmp = MonteCarloMoments(t2, dir, nMax);
		for (u32 i = 0; i < mcMoments.size(); ++i)
			mcMoments[i] += tmp[i];
	}

	// Test difference for each order
	for (int i = nMin; i <= nMax; ++i) {
		f32 analyticalMoment = moments[i];
		//f32 mcMoment = shvals[i * (i + 1)] * weight;
		f32 mcMoment = mcMoments[i];

		f32 error = analyticalMoment - mcMoment;
		error *= error;

		if (error > 1e-2f) {
			errors++;
			LogInfo("Order ", i, " : (AM) ", analyticalMoment, " | ", mcMoment, " (MC). L2 : ", error);
		}
	}


	LogInfo("Computing Eigen Axial Moment.");
	// Compare with Eigen Axial moments
	auto momentsEigen = AxialMomentEigen(P, dir, nMax);

	// Test difference for each order
	for (int i = nMin; i <= nMax; ++i) {
		f32 analyticalMoment = moments[i];
		//f32 mcMoment = shvals[i * (i + 1)] * weight;
		f32 eigenMoment = -momentsEigen[i];

		f32 error = analyticalMoment - eigenMoment;
		error *= error;

		if (error > 1e-2f) {
			errors++;
			LogInfo("Order ", i, " : (AM) ", analyticalMoment, " | ", eigenMoment, " (EI). L2 : ", error);
		}
	}


	LogInfo("Comparing axial moments on several directions.");
	// Doing Moments on several directions
	std::vector<vec3f> directions;
	u32 dirCount = 2 * nMax + 1;
	Sampling::SampleSphereBluenoise(directions, dirCount);

	std::vector<f32> momentsDirs = P.AxialMoments(directions);
	auto momentsDirsEigen = AxialMomentsEigen(P, directions);

	// Test difference for each order
	for (int i = nMin; i < (int) dirCount; ++i) {
		for (int j = 0; j < nMax + 1; ++j) {
			f32 analyticalMoment = momentsDirs[i*(nMax+1)+j];
			//f32 mcMoment = shvals[i * (i + 1)] * weight;
			f32 eigenMoment = -momentsDirsEigen[i*(nMax+1)+j];

			f32 error = analyticalMoment - eigenMoment;
			error *= error;

			if (error > 1e-2f) {
				errors++;
				LogInfo("Dir ", i, " Order ", j, " : (AM) ", analyticalMoment, " | ", eigenMoment, " (EI). L2 : ", error);
			}
		}
	}

	return errors;
}

int TestZHIntegral(const std::vector<vec3f> &v, int n) {
	const u32 dirCount = 2 * (u32) n + 1;
	const u32 order = (dirCount - 1) / 2 + 1;
	const u32 mrows = order*order;
	const u32 mcols = dirCount*order;

	LogInfo("Testing Axial Moment polygon unit integral. Order ", n);

	std::vector<vec3f> sampleDirs;
	//Sampling::SampleSphereRandom(sampleDirs, dirCount);
	Sampling::SampleSphereBluenoise(sampleDirs, dirCount);

	// Get AP matrix from product of the Zonal Weights, and Zonal Expansion coeffs
	//f32 *AP = new f32[mrows*mcols]();
	//ComputeAPMatrix((f32*) &sampleDirs[0], dirCount, AP); // of size mcols*mrows
	const auto ZW = ZonalWeightsEigen(sampleDirs);
	const auto Y = ZonalExpansionEigen(sampleDirs);
	const auto A = computeInverseEigen(Y);
	const auto AP = A*ZW;
	
	int errors = 0;

	// Compute product of AxialMoments and AP matrix
	std::vector<vec3f> verts(v);
	Rectangle rect(verts);
	
	for (u32 i = 0; i < verts.size(); ++i)
		verts[i].Normalize();
	
	Polygon P(verts);
	//std::vector<f32> moments = P.AxialMoments(sampleDirs); // of size mcols
	const auto moments = AxialMomentsEigen(P, sampleDirs);

	//std::vector<f32> shvals(mrows, 0.f); // order^2 SH coefficients
	//MatrixProduct(AP, &moments[0], &shvals[0], mrows, mcols, 1);
	const Eigen::MatrixXf shvals = AP * moments;
	Eigen::Matrix<typename float, Eigen::Dynamic, Eigen::Dynamic> tmp = shvals;
	f32 *rawAP = tmp.data();

	// Compare with full SH expansion
	std::vector<f32> shvalsGT(mrows, 0.f);
	f32 weight = rect.IntegrateAngularStratification(vec3f(0.f), vec3f(0, 1, 0), 10000, shvalsGT, order);

	// Test difference for each coeff
	for (int i = 0; i < (int) mrows; ++i) {
		f32 analyticalMoments = -shvals(i);
		f32 sh = shvalsGT[i] * weight;

		f32 error = analyticalMoments - sh;
		error *= error;

		if (error > 1e-2f) {
			errors++;
			LogInfo("Coeff ", i, " : (AM) ", analyticalMoments, " | ", sh, " (SH). L2 : ", error);
		}
	}

	//delete[] AP;
	std::cout << std::endl;

	return errors;
}

bool DoTests() {
	return true;

	const int nMin = 0;
	const int nMax = 3;

	std::vector<vec3f> verts;
	verts.push_back(vec3f(-0.5, -0.5, 1.0));
	verts.push_back(vec3f(0.5, -0.5, 1.0));
	verts.push_back(vec3f(0.5, 0.5, 1.0));
	verts.push_back(vec3f(-0.5, 0.5, 1.0));

	// Test for normal direction
	LogInfo("Testing for axis (0, 1, 0)");
	vec3f w = Random::Vec3f();
	w.Normalize();
	int fail = TestMoments(w, verts, nMin, nMax);

	// Test for a grazing direction. This should give zero moments for
	// odd orders.
	//LogInfo("Testing for axis (1, 0, 0)");
	//w = vec3f(1, 0, 0);
	//fail += (int) TestMoments(w, verts, nMin, nMax);

	LogErr(fail, " errors while testing moments.\n");

	fail = TestZHValidation(nMax);

	LogErr(fail, " errors while testing ZH validation.\n");

	fail = TestZHIntegral(verts, nMax);

	LogErr(fail, " errors while testing ZH integration.\n");
	
	return false;
}

bool MakeLights(Scene *scene) {
#if 0
	PointLight::Desc light;
	PointLight::Handle light_h;
	// Lights
	light.position = vec3f(9.5, 5, 10);
	light.Ld = vec3f(1.5, 1, 0);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if (light_h < 0) goto err;

	light.position = vec3f(90, 15, -3);
	light.Ld = vec3f(1.2, 1.2, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if (light_h < 0) goto err;

	light.position = vec3f(-90, 15, -3);
	light.Ld = vec3f(1.2, 1.2, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if (light_h < 0) goto err;
	/*
	light.position = vec3f(40.5, 8, -10);
	light.Ld = vec3f(2,2,2);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;
	*/
	light.position = vec3f(-5, 10, -10);
	light.Ld = vec3f(1.5, 0.8, 1.2);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if (light_h < 0) goto err;

	light.position = vec3f(20, 30, 0);
	light.Ld = vec3f(3, 3, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if (light_h < 0) goto err;
#endif

	AreaLight::Desc al;

	al.position = alPos;
	al.width = vec2f(8.f, 6.f);
	al.rotation = vec3f(0, 0, 0);
	al.Ld = vec3f(2.0, 1.5, 1);

	alh = scene->Add(al);
	if(alh < 0) goto area_err;

#if 0
	al.position = vec3f(60, 0, 0);
	al.width = vec2f(10.f, 2.f);
	al.rotation = vec3f(0, 0, 0);
	al.Ld = vec3f(1, 0.8, 0);

	alh2 = scene->Add(al);
	if(alh2 < 0) goto area_err;

	al.position = alPos2;
	al.width = vec2f(60.f, 18.f);
	al.rotation = vec3f(0, M_PI, 0);
	al.Ld = vec3f(1.5,1,2);

	alh3 = scene->Add(al);
	if(alh3 < 0) goto area_err;
#endif

	return true;
	area_err: {
		LogErr("Couldn't add area light to scene.");
		return false;
	}
}

bool Init(Scene *scene) {
	if(!MakeLights(scene))
		return false;

	f32 hWidth = 200;
	f32 texRepetition = hWidth/5;
	f32 pos[] = {
		-hWidth, 0, -hWidth,
		-hWidth, 0, hWidth,
		hWidth, 0, hWidth,
		hWidth, 0, -hWidth
	};
	f32 colors[] = {
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
		1, 1, 1, 1
	};
	f32 texcoords[] = {
		0, 0,
		0, texRepetition,
		texRepetition, texRepetition,
		texRepetition, 0
	};

	f32 normals[] = {
		0, 1, 0,
		0, 1, 0,
		0, 1, 0,
		0, 1, 0
	};

	u32 idx[] = {
		0, 1, 2, 0, 2, 3
	};

	Skybox::Desc skyd;
	skyd.filenames[0] = "../../data/skybox/sky1/right.png";
	skyd.filenames[1] = "../../data/skybox/sky1/left.png";
	skyd.filenames[2] = "../../data/skybox/sky1/down.png";
	skyd.filenames[3] = "../../data/skybox/sky1/up.png";
	skyd.filenames[4] = "../../data/skybox/sky1/back.png";
	skyd.filenames[5] = "../../data/skybox/sky1/front.png";
	Skybox::Handle sky = scene->Add(skyd);
	if(sky < 0) {
		LogErr("Error loading skybox.");
		return false;
	}
	scene->SetSkybox(sky);


	
	Render::Mesh::Desc mdesc("TestMesh", false, 6, idx, 4, pos, normals, texcoords, nullptr, nullptr, colors);
	Render::Mesh::Handle plane_mesh = Render::Mesh::Build(mdesc);
	if(plane_mesh < 0) {
		LogErr("Error creating test mesh");
		return false;
	}

	Render::Mesh::Handle sphere = Render::Mesh::BuildSphere();
	if (sphere < 0) {
		LogErr("Error creating sphere mesh");
		return false;
	}

	sh1.Init(scene, nBand);
	sh1.AddAreaLight(alh);
	//sh1.AddAreaLight(alh2);
	//sh1.AddAreaLight(alh3);
	


	Object::Desc odesc(Render::Shader::SHADER_3D_MESH);	
	{
		odesc.ClearSubmeshes();

		Material::Desc mat_desc(col3f(0.1f,0.1f,0.1f), col3f(1,1,1), col3f(1,1,1), 0.65f);
		mat_desc.diffuseTexPath = "../../data/concrete.png";
		mat_desc.normalTexPath = "../../data/concrete_nm.png";
		mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
		mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";
		Material::Handle mat = scene->Add(mat_desc);
		if(mat < 0) {
			LogErr("Error adding material");
			return false;
		}

		odesc.AddSubmesh(plane_mesh, mat);
		odesc.Identity();
		odesc.Translate(vec3f(0,-1.5f,0));

		Object::Handle obj = scene->Add(odesc);
		if(obj < 0) {
			LogErr("Error registering plane");
			return false;
		}
	}
#if 0
	const vec3f arrayPos(20, 0, 0);

	const int sphere_n = 10;
	const int sphere_j = 8;
	for(int j = 0; j < sphere_j; ++j) {
		for(int i = 0; i < sphere_n; ++i) {
			f32 fi = pow((i+1) / (f32)sphere_n, 0.4f);
			f32 fj = j / (f32)sphere_j;

			odesc.ClearSubmeshes();

			odesc.Identity();
			odesc.Translate(arrayPos + vec3f(-2 + i * 3.f, 0.f, -8 + j * 3.f));
			odesc.Rotate(vec3f(0, M_PI_OVER_TWO * i, 0));

			Material::Desc mat_desc(col3f(0.0225 + fj * 0.16, 0.0735, 0.19125 - fj * 0.16),
									col3f(0.0828 + fj * 1.05, 0.17048, 1.9038 - fj * 1.05),
									col3f(0.08601+ fj * 0.16, 0.13762, 0.25678- fj * 0.16),
									0.001f + 0.984f * fi);
			mat_desc.normalTexPath = "../../data/wave_nm.png";
			//mat_desc.specularTexPath = "data/metal_spec.png";
			mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
			mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";

			Material::Handle mat = scene->Add(mat_desc);
			if(mat < 0) {
				LogErr("Error adding material");
				return false;
			}

			odesc.AddSubmesh(sphere, mat);

			Object::Handle sphere_object = scene->Add(odesc);
			if(sphere_object < 0) {
				LogErr("Error registering sphere ", i, ", ", j, ".");
				return false;
			}
		}
	}
#endif
	return true;
}

void UpdateUI(float dt) {
	const Device &device = GetDevice();
	vec2i ws = device.windowSize;

	ImGui::SetNextWindowPos(ImVec2((f32)ws.x - 200, (f32)ws.y - 420));
	ImGui::SetNextWindowSize(ImVec2(190, 410));

	ImGui::Begin("TweakPanel", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	ImGui::Checkbox("Auto Update", &autoUpdate);
	ImGui::Checkbox("SH Vis Normalization", &shNormalization);
	ImGui::Checkbox("Light Rotation", &movement);
	ImGui::Checkbox("Convergence Tests", &doTests);

	ImGui::Text("GGX Exponent :");
	ImGui::SliderFloat("shininess", &GGXexponent, 0.f, 0.9f);

	ImGui::Text("Sample Count :");
	ImGui::SliderInt("samples", &numSamples, 1, 30000);

	ImGui::Text("Integration Method :");
	ImGui::RadioButton("Uniform Random", &method, 0);
	ImGui::RadioButton("Angular Stratification", &method, 1);
	ImGui::RadioButton("Spherical Rectangles", &method, 2);
	ImGui::RadioButton("Tri Sampling Unit", &method, 3);
	ImGui::RadioButton("Tri Sampling WS", &method, 4);
	ImGui::RadioButton("Tangent Plane", &method, 5);
	ImGui::RadioButton("Tangent Plane Unit", &method, 6);
	ImGui::RadioButton("Arvo Moments", &method, 7);
	ImGui::RadioButton("LTC Analytic", &method, 8);

	ImGui::Text("BRDF :");
	ImGui::RadioButton("Diffuse", &brdfMethod, (int)BRDF_Diffuse);
	ImGui::RadioButton("GGX", &brdfMethod, (int)BRDF_GGX);
	ImGui::RadioButton("Both", &brdfMethod, (int)BRDF_Both);

	ImGui::End();
}

void FixedUpdate(Scene *scene, float dt) {
	AreaLight::Desc *light = scene->GetLight(alh);
	if (light) {
		static float dir = 1.f;

		if (movement) {
			if (light->rotation.x >= (M_PI * 0.8f) || light->rotation.x <= (-M_PI * 0.28f))
				dir *= -1.f;

			light->rotation.x += dir * dt * M_PI * 0.25f;
		}
	}

	// 1 frame lag here. The light rotation doesn't get updated right away, it gets updated before the render
	// add a Commit() function for those kind of changes ?

	if (autoUpdate) {
		sh1.SetGGXExponent(GGXexponent);
		sh1.UseSHNormalization(shNormalization);
		sh1.SetIntegrationMethod(AreaLightIntegrationMethod(method));
		sh1.SetBRDF(AreaLightBRDF(brdfMethod));
		sh1.SetSampleCount(numSamples);
		sh1.Recompute();
	}
}

void Update(Scene *scene, float dt) {
	const Device &device = GetDevice();
	vec2i mouseCoords = vec2i(device.GetMouseX(), device.GetMouseY());
	vec2i ws = device.windowSize;

	static f32 t = 0;
	
	UpdateUI(dt);
	
	if (device.IsMouseDown(MouseButton::MB_Left) && !ImGui::GetIO().WantCaptureMouse) {
		vec4f pos = Render::FBO::ReadGBuffer(Render::FBO::GBufferAttachment::WORLDPOS, mouseCoords.x, mouseCoords.y);
		vec4f nrm = Render::FBO::ReadGBuffer(Render::FBO::GBufferAttachment::NORMAL, mouseCoords.x, mouseCoords.y);
		
		sh1.SetGGXExponent(GGXexponent);
		sh1.UseSHNormalization(shNormalization);
		sh1.SetIntegrationMethod(AreaLightIntegrationMethod(method));
		sh1.SetBRDF(AreaLightBRDF(brdfMethod));
		sh1.SetSampleCount(numSamples);
		sh1.UpdateCoords(vec3f(pos.x, pos.y, pos.z), vec3f(nrm.x, nrm.y, nrm.z));
		if(doTests) sh1.TestConvergence("data/arealight", 500, 4000, 0.25f);
	}

	if (device.IsKeyHit(K_R)) {
		sh1.SetGGXExponent(GGXexponent);
		sh1.UseSHNormalization(shNormalization);
		sh1.SetIntegrationMethod(AreaLightIntegrationMethod(method));
		sh1.SetBRDF(AreaLightBRDF(brdfMethod));
		sh1.SetSampleCount(numSamples);
		sh1.Recompute();
	}

	t += dt;
}

void renderFunc(Scene *scene) {
}


int main() {
	Log::Init();


	Device &device = GetDevice();
	if (!device.Init(Init)) {
		printf("Error initializing Device. Aborting.\n");
		device.Destroy();
		system("PAUSE");
		return 1;
	}

	if (!DoTests()) {
		device.Destroy();
		Log::Close();
		system("PAUSE");
		return 0;
	}

	device.SetUpdateFunc(Update);
	device.SetFixedUpdateFunc(FixedUpdate);

	device.Run();

	device.Destroy();
	Log::Close();
    return 0;
}
