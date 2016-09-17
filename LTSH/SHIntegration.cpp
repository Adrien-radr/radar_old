#include "SHIntegration.h"
#include "src/common/SHEval.h"
#include "src/common/sampling.h"
#include "Tests.h"
#include "AxialMoments.hpp"

#include <algorithm>

//#pragma optimize("",off)
static const float g_SubdivThreshold = 0.866f; // subdivide triangles having edges larger than 60 degrees

bool SHInt::Init(Scene *scene, u32 band) {
	nBand = band;
	nCoeff = band * band;
	gameScene = scene;

	integrationPos = vec3f(0, 0, 0);
	integrationNrm = vec3f(0, 1, 0);

	shCoeffs.resize(nCoeff);
	integrationSurface.resize(12);

	Material::Desc mat_desc(col3f(0.6f, 0.25f, 0), col3f(1, 0.8f, 0), col3f(1, 0, 0), 0.4f);
	mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
	mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";
	mat_desc.dynamic = true;
	mat_desc.gbufferDraw = false;

	material = scene->Add(mat_desc);
	if (material < 0) {
		LogErr("Error adding SH Vis material");
		return false;
	}

	mat_desc.uniform.Ka = col3f(0.25f, 0.f, 0.6f);
	mat_desc.uniform.Kd = col3f(0.8f, 0.f, 1.f);
	mat_desc.uniform.Ks = col3f(0, 0, 1);

	integrationMaterial = scene->Add(mat_desc);
	if (integrationMaterial < 0) {
		LogErr("Error adding SH Integration material.");
		return false;
	}

	Object::Desc od(Render::Shader::SHADER_3D_MESH);
	shObj = scene->Add(od);
	if (shObj < 0) {
		LogErr("Error adding SH Vis object");
		return false;
	}

	integrationObj = scene->Add(od);
	if (integrationObj < 0) {
		LogErr("Error adding SH Integration object");
		return false;
	}

	ltcMat = new float[32 * 32 * 4]; // 32x32 RGBA32F
	ltcAmp = new float[32 * 32 * 2]; // 32x32 RG32F

	const u32 order = nBand;
	const u32 dirCount = 2 * (u32) (nBand-1) + 1;
	Sampling::SampleSphereBluenoise(sampleDirs, dirCount);

	const auto ZW = ZonalWeightsEigen(sampleDirs);
	const auto Y = ZonalExpansionEigen(sampleDirs);
	const auto A = computeInverseEigen(Y);
	APMap = A*ZW;
	//Eigen::Matrix<typename float, Eigen::Dynamic, Eigen::Dynamic> APMap = AP;
	//APMatrix = new float[order*order*order*dirCount]();
	//std::memcpy(APMatrix, APMap.data(), sizeof(f32) * order * order * order * dirCount);

	//ComputeAPMatrix((f32*) &sampleDirs[0], dirCount, APMatrix); // of size mcols*mrows

	// precompute random values
	triPool.Init(4096, [](vec2f v) { 
		const f32 sqrtu1 = std::sqrtf(v.x);
		return vec2f(1.f - sqrtu1, v.y * sqrtu1); 
	});

	// Get ltc data from GL textures
	Material::Data *matData = gameScene->GetMaterial(material);

	Render::Texture::GetData(matData->ltcMatrix, Render::Texture::RGBA32F, ltcMat);
	Render::Texture::GetData(matData->ltcAmplitude, Render::Texture::RG32F, ltcAmp);

	shNormalization = false;
	GGXexponent = 0.5f;
	integrationMethod = TriSamplingUnit;
	numSamples = 1024;
	usedBRDF = BRDF_Diffuse;

	return true;
}

SHInt::~SHInt() {
	if(ltcMat) delete[] ltcMat;
	if(ltcAmp) delete[] ltcAmp;
	//if(APMatrix) delete[] APMatrix;
}

void SHInt::Recompute() {
	integrationView = gameScene->GetCamera().position - integrationPos;
	integrationView.Normalize();

	IntegrateAreaLights();
}

void SHInt::UpdateCoords(const vec3f &position, const vec3f &normal) {
	initialized = true; // only start simulation if the user clicked somewhere at lease once

	// place at position, shifted in the normal's direction a bit
	vec3f pos = position + normal * 0.1f;

	integrationPos = position;
	integrationNrm = normal;

	Object::Desc *od = gameScene->GetObject(shObj);
	od->Identity();
	od->Translate(pos);

	// Call Integration over new position
	Recompute();
}

void SHInt::TestConvergence(const std::string & outputFileName, u32 numPasses, u32 numSamplesEqual, f32 numSecondsEqual) {
	std::vector<float> shvals(nCoeff);
	std::vector<float> shcmp(nCoeff);
	std::vector<float> shtmp(nCoeff);
	std::fill_n(shvals.begin(), nCoeff, 0.f);

	const u32 sampleSteps = 150;
	const u32 sampleCountStep = 4;
	const u32 maximumSamples = 5000;
	const u32 nMethods = (u32) LTCAnalytic - (u32) UniformRandom;

	const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[0]);

	if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {		
		AreaLightIntegrationMethod prevMethod = integrationMethod;
		u32 prevSamples = numSamples;

		// Construct Rectangle for numerical techniques
		const Rectangle arect = AreaLight::GetRectangle(*al);

		// Construct planar rectangle for Tangent Plane sampling
		PlanarRectangle prect;
		prect.InitBary(arect, integrationPos);

		PlanarRectangle uprect;
		uprect.InitUnit(arect, integrationPos);

		// COnstruct spherical rectangle for Spherical Rectangles sampling strategy
		SphericalRectangle sphrect;
		sphrect.Init(arect, integrationPos);

		// Construct Polygon for Axial Moments
		std::vector<vec3f> verts(4);
		AreaLight::GetVertices(*al, &verts[0]);
		for (int i = 0; i < 4; ++i) {
			verts[i] -= integrationPos;
			verts[i].Normalize();
		}
		Polygon P(verts);

		std::vector<f64> l1[nMethods];
		std::vector<f64> l2[nMethods];
		std::vector<f64> l3[nMethods];
		std::vector<f64> l4[nMethods];
		std::vector<f64> t1[nMethods];

		static const char* methodNames[LTCAnalytic] = {
			"Random",
			"AS",
			"SR",
			"TriUnit",
			"TriWS",
			"PlaneBary",
			"PlaneUnit",
			"ArvoMoments"
		};

		auto WriteResult = [&](CSV &csv) {
			csv.WriteCell("L2-Norm Mean"); csv.WriteNewLine();
			csv.WriteCell("SampleCount:");
			for (u32 i = 1; i <= sampleSteps; ++i) {
				csv.WriteCell<int>(i*sampleCountStep);
			}
			csv.WriteNewLine();
			for (u32 j = 0; j < nMethods; ++j) {
				csv.WriteCell(methodNames[j]);
					for (u32 i = 0; i < sampleSteps; ++i) {
						csv.WriteCell<f64>(l1[j][i]);
					}
				csv.WriteNewLine();
			}
			csv.WriteNewLine();
			csv.WriteCell("L2-Norm Variance"); csv.WriteNewLine();
			csv.WriteCell("SampleCount:");
			for (u32 i = 1; i <= sampleSteps; ++i) {
				csv.WriteCell<int>(i*sampleCountStep);
			}
			csv.WriteNewLine();
			for (u32 j = 0; j < nMethods; ++j) {
				csv.WriteCell(methodNames[j]);
					for (u32 i = 0; i < sampleSteps; ++i) {
						csv.WriteCell<f64>(l2[j][i]);
					}
				csv.WriteNewLine();
			}

			// DC
			csv.WriteNewLine();
			csv.WriteCell("DC Only");csv.WriteNewLine();
			csv.WriteNewLine();
			csv.WriteCell("L2-Norm Mean"); csv.WriteNewLine();
			csv.WriteCell("SampleCount:");
			for (u32 i = 1; i <= sampleSteps; ++i) {
				csv.WriteCell<int>(i*sampleCountStep);
			}
			csv.WriteNewLine();
			for (u32 j = 0; j < nMethods; ++j) {
				csv.WriteCell(methodNames[j]);
					for (u32 i = 0; i < sampleSteps; ++i) {
						csv.WriteCell<f64>(l3[j][i]);
					}
				csv.WriteNewLine();
			}
			csv.WriteNewLine();
			csv.WriteCell("L2-Norm Variance"); csv.WriteNewLine();
			csv.WriteCell("SampleCount:");
			for (u32 i = 1; i <= sampleSteps; ++i) {
				csv.WriteCell<int>(i*sampleCountStep);
			}
			csv.WriteNewLine();
			for (u32 j = 0; j < nMethods; ++j) {
				csv.WriteCell(methodNames[j]);
					for (u32 i = 0; i < sampleSteps; ++i) {
						csv.WriteCell<f64>(l4[j][i]);
					}
				csv.WriteNewLine();
			}

			// Timings
			csv.WriteNewLine();
			csv.WriteCell("Times"); csv.WriteNewLine();
			csv.WriteNewLine();
			csv.WriteCell("SampleCount:");
			for (u32 i = 1; i <= sampleSteps; ++i) {
				csv.WriteCell<int>(i*sampleCountStep);
			}
			csv.WriteNewLine();
			for (u32 j = 0; j < nMethods; ++j) {
				csv.WriteCell(methodNames[j]);
					for (u32 i = 0; i < sampleSteps; ++i) {
						csv.WriteCell<f64>(t1[j][i]);
					}
				csv.WriteNewLine();
			}

			csv.Close();
		};

		auto ResetArrays = [&]() {
			for (u32 m = (u32) UniformRandom; m < nMethods; ++m) {
				l1[m].clear();
				l2[m].clear();
				l3[m].clear();
				l4[m].clear();
				t1[m].clear();
				l1[m].resize(sampleSteps, 0.);
				l2[m].resize(sampleSteps, 0.);
				l3[m].resize(sampleSteps, 0.);
				l4[m].resize(sampleSteps, 0.);
				t1[m].resize(sampleSteps, 0.);
			}
		};

		auto TestMethods = [&](const std::vector<f32> &GT, f32 mEpsilon, u32 mSamples, f64 mSeconds) {
			const f64 maxTime = mSeconds > 0.f ? mSeconds : 60.f; // 60 seconds max by default
			const u32 maxSamples = mSamples > 0 ? mSamples : maximumSamples; // 100k samples max by default
			const f32 epsilon = mEpsilon > 0.f ? mEpsilon : 1e-4f; // 1e-4 epsilon by default


			for (u32 m = (u32) UniformRandom; m < nMethods; ++m) {
				integrationMethod = (AreaLightIntegrationMethod) m;

				LogInfo("Testing ", methodNames[m]);

				// loop incrementing the number of samples used
				for (u32 i = 0; i < sampleSteps; ++i) {
					numSamples = (i+1) * sampleCountStep;

					f64 dc_mean_err = 0.;
					f64 dc_mean_err2 = 0.;
					f64 dc_var_err = 0.;
					f64 mean_err = 0.;
					f64 mean_err2 = 0.;
					f64 var_err = 0.;
					f64 mean_timing = glfwGetTime();

					// loop over subpasses averaging mean and variance for the current number of samples
					for (u32 j = 0; j < numPasses; ++j) {
						std::fill_n(shtmp.begin(), nCoeff, 0.f);

						f32 wt = IntegrateLightSmart(*al, arect, sphrect, P, prect, uprect, shtmp);

						// normalize, accumulate and compare with GT
						f64 error = 0.f, error2 = 0.f;
						for (u32 i = 0; i < nCoeff; ++i) {
							shvals[i] = shtmp[i] * wt;

							// L2 norm
							f64 nrm = GT[i] - shvals[i];
							error += nrm;
							error2 += nrm * nrm;
						}
						//error = std::abs(error);
						error2 = std::sqrt(error2);

						mean_err += error2;
						mean_err2 += error2 * error2;

						error2 = GT[0] - shvals[0];
						error2 = std::sqrt(error2 * error2); // L2 norm of DC
						dc_mean_err += error2;
						dc_mean_err2 += error2 * error2;
					}

					mean_timing = glfwGetTime() - mean_timing;
					mean_timing /= numPasses;

					mean_err /= numPasses;
					mean_err2 /= numPasses;
					dc_mean_err /= numPasses;
					dc_mean_err2 /= numPasses;

					var_err = mean_err2 - mean_err * mean_err;
					dc_var_err = dc_mean_err2 - dc_mean_err * dc_mean_err;

					l1[m][i] = mean_err;
					l2[m][i] = var_err;
					l3[m][i] = dc_mean_err;
					l4[m][i] = dc_var_err;
					t1[m][i] = mean_timing;
				}
			}
		};

		LogInfo("Tests begin, averaging over ", numPasses, " passes.");

		// Computing Ground Truth
		std::vector<f32> shGT(nCoeff);
		{
			integrationMethod = ArvoMoments;
			numSamples = 1;

			f32 wt = IntegrateLight(*al, shGT);
			for (u32 j = 0; j < nCoeff; ++j) {
				shGT[j] *= wt;
			}
		}

		// Test convergence rate
		const bool testConvergence = true;
		if(testConvergence) {
			CSV csv;
			std::string convergenceFile = outputFileName + "_convergence.csv";
			if (!csv.Open(convergenceFile, CSV::OpenWrite)) {
				return;
			}

			LogInfo("Epsilon Test (e=", 5e-3f, ")");

			ResetArrays();

			TestMethods(shGT, 5e-3f, 0, 0.0);

			// Write result, normalized across passes
			WriteResult(csv);
		}

		// Test Equal-SampleCount
		const bool testEqualSample = false;
		if (testEqualSample) {
			CSV csv;
			std::string convergenceFile = outputFileName + "_equalSamples.csv";
			if (!csv.Open(convergenceFile, CSV::OpenWrite)) {
				return;
			}

			LogInfo("Equal samples Test (s=", numSamplesEqual, ")");

			ResetArrays();

			TestMethods(shGT, 0.f, numSamplesEqual, 0.0);

			// Write result, normalized across passes
			WriteResult(csv);
		}

		// Test Equal-Time
		const bool testEqualTime = false;
		if (testEqualTime) {
			CSV csv;
			std::string convergenceFile = outputFileName + "_equalTime.csv";
			if (!csv.Open(convergenceFile, CSV::OpenWrite)) {
				return;
			}

			LogInfo("Equal time Test (t=", numSecondsEqual, "s)");

			ResetArrays();

			TestMethods(shGT, 0.f, 0, (f64) numSecondsEqual);

			// Write result, normalized across passes
			WriteResult(csv);
		}

		LogInfo("Tests End.");

		// Reset params as it was
		integrationMethod = prevMethod;
		numSamples = prevSamples;
	}

}

void SHInt::UpdateData(const std::vector<float> &coeffs) {
	std::fill_n(shCoeffs.begin(), nCoeff, 0.f);
	std::copy_n(coeffs.begin(), nCoeff, shCoeffs.begin());

	// Generate SH Visu
	Object::Desc *od = gameScene->GetObject(shObj);

	od->DestroyData();
	od->ClearSubmeshes();

	Render::Mesh::Handle shVis = Render::Mesh::BuildSHVisualization(&shCoeffs[0], nBand, "SHVis1", shNormalization);
	if (shVis < 0) {
		LogErr("Error creating sh vis mesh");
		return;
	}

	od->AddSubmesh(shVis, material);

	// Generate integration object
	od = gameScene->GetObject(integrationObj);

	od->DestroyData();
	od->ClearSubmeshes();

	static u32 indices[] = { 0, 1, 2, 3, 4, 5, 6, 8, 7, 9, 11, 10 };
	static vec3f normals[12];
	for (u32 i = 0; i < 6; ++i) {
		normals[i] = integrationSurface[i] - integrationPos;
		normals[i].Normalize();
	}
	for (u32 i = 6; i < 12; ++i) {
		normals[i] = integrationPos - integrationSurface[i];
		normals[i].Normalize();
	}

	Render::Mesh::Desc d("IntegrationPoly", false, 12, indices, 12, (f32*) &integrationSurface[0], (f32*)normals);

	Render::Mesh::Handle intMesh = Render::Mesh::Build(d);
	if (intMesh < 0) {
		LogErr("Error creating integration polygon visualisation.");
		return;
	}

	od->AddSubmesh(intMesh, integrationMaterial);
}

f32 SHInt::IntegrateTrisLTC(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);

	// Integrate the polygon for diffuse & ggx brdf
	f32 contrib = 0.f;

	if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
		mat3f MinvDiff; // identity for diffuse
		contrib += Render::BRDF::LTC_Evaluate(integrationNrm, integrationView, integrationPos, MinvDiff, points, false);
	}
	if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
		// indexing variables
		const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
		const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);
		const f32 Ks = 1.f;

		const vec2f ltcCoords = Render::BRDF::LTC_Coords(NdotV, roughness, 32);
		const mat3f MinvSpec = Render::BRDF::LTC_Matrix(ltcMat, ltcCoords, 32);
		const vec2f schlick = BilinearLookup<vec2f>(ltcAmp, ltcCoords, vec2i(32, 32));

		f32 spec = Render::BRDF::LTC_Evaluate(integrationNrm, integrationView, integrationPos, MinvSpec, points, false);
		spec *= Ks * schlick.x + (1.f - Ks) * schlick.y;

		contrib += spec;
	}

	// project AL's center with integration weight onto SH
	vec3f alBary = al.position - integrationPos;
	alBary.Normalize();

	//for (u32 i = 0; i < 4; ++i) {
		//vec3f rayDir = points[i] - integrationPos;
		//rayDir.Normalize();

	vec3f rayDir = alBary;
	SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

	for (u32 j = 0; j < nCoeff; ++j) {
		shvals[j] += shtmp[j] * contrib;
	}
	//}

	return 1.f / (2.f * M_PI);
}

f32 SHInt::IntegrateTrisSampling(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals, bool wsSampling) {
	static const u32 triIdx[2][3] = { {0, 1, 2}, {0, 2, 3} };
	static const u32 numTri = 2; // 2 tris per arealight

	const u32 sampleCount = numSamples / numTri;

	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);
	const vec3f pN(al.plane.x, al.plane.y, al.plane.z);

	const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
	const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);

	Triangle subdivided[4];
	u32 numSubdiv;

	// Accum
	for (u32 tri = 0; tri < numTri; ++tri) {
		if (wsSampling) {
			subdivided[0].InitWS(pN, points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

			for (u32 i = 0; i < 2; ++i) {
				integrationSurface[i * 6 + tri * 3 + 0] = subdivided[0].q0 + integrationPos;
				integrationSurface[i * 6 + tri * 3 + 1] = subdivided[0].q1 + integrationPos;
				integrationSurface[i * 6 + tri * 3 + 2] = subdivided[0].q2 + integrationPos;
			}

			numSubdiv = 1;
		} 
		else {
			Triangle triangle;
			triangle.InitUnit(points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

			for (u32 i = 0; i < 2; ++i) {
				integrationSurface[i * 6 + tri * 3 + 0] = triangle.q0 + integrationPos;
				integrationSurface[i * 6 + tri * 3 + 1] = triangle.q1 + integrationPos;
				integrationSurface[i * 6 + tri * 3 + 2] = triangle.q2 + integrationPos;
			}

			// Subdivide projected triangle if too large for robust integration
			if (triangle.distToOrigin() > g_SubdivThreshold || sampleCount < 4) {
				subdivided[0] = triangle;
				numSubdiv = 1;
			}
			else {
				numSubdiv = triangle.Subdivide4(subdivided);
			}
		}
			
		const u32 sc = sampleCount / numSubdiv;

		// Loop over the subdivided triangles (or the single unsubdivided triangle)
		for (u32 subtri = 0; subtri < numSubdiv; ++subtri) {
			const Triangle &sub = subdivided[subtri];

			// Sample triangle
			for (u32 i = 0; i < sc; ++i) {
				//vec2f randVec = Random::Vec2f();
				vec2f bary = triPool.Next();

				const vec3f &pt = sub.SamplePointBary(bary.x, bary.y);
				vec3f rayDir;
				f32 invPdf = sub.SampleDir(rayDir, pt) * sub.area / sc;
				//f32 NdotL = rayDir.Dot(integrationNrm);

				// Fill SH with impulse from that direction
				if (invPdf > 0.f) {// && NdotL > 0.f) {
					SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

					/*
					f32 contrib = 0.f;

					if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
						contrib += Render::BRDF::Diffuse();
					}
					if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
						vec3f H = integrationView + rayDir;
						H.Normalize();

						const f32 NdotH = std::max(0.f, integrationNrm.Dot(H));
						const f32 LdotH = std::max(0.f, rayDir.Dot(H));
						contrib += Render::BRDF::GGX(NdotL, NdotV, NdotH, LdotH, roughness, vec3f(1, 1, 1)).x;
					}
					*/

					for (u32 j = 0; j < nCoeff; ++j) {
						shvals[j] += shtmp[j] * invPdf;// *NdotL * contrib;
					}
				}
			}
		}
	}
	
	return 1.f;
}

f32 SHInt::IntegrateArvoMoments(const Polygon &P, std::vector<f32> &shvals) {
	const u32 dirCount = 2 * (u32) (nBand-1) + 1;
	const u32 order = nBand;// (dirCount - 1) / 2 + 1;
	const u32 mrows = order*order;
	const u32 mcols = order*dirCount;
	
	const auto moments = AxialMomentsEigen(P, sampleDirs);
	//std::vector<f32> moments = P.AxialMoments(sampleDirs);


	auto SHMap = Eigen::Map<Eigen::MatrixXf>(&shvals[0], mrows, 1);
	SHMap = APMap * moments;
	//Eigen::Matrix<typename f32, Eigen::Dynamic, Eigen::Dynamic> SHMap = APMap * moments;
	//const f32 *ptr = SHMap.data();
	//std::memcpy(&shvals[0], ptr, sizeof(f32) * mrows);

	//MatrixProduct(APMatrix, &moments[0], &shvals[0], mrows, mcols, 1);

	return 1.f;
}

f32 SHInt::IntegrateLightSmart(const AreaLight::UniformBufferData &al, const Rectangle &rect, const SphericalRectangle &sphrect, const Polygon &P, const PlanarRectangle &prect, const PlanarRectangle &uprect, std::vector<f32> &shvals) {
	f32 ret;
	
	switch (integrationMethod) {
	case UniformRandom:
		ret = rect.IntegrateRandom(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case AngularStratification:
		ret = rect.IntegrateAngularStratification(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case SphericalRectangles:
		ret = sphrect.Integrate(integrationNrm, numSamples, shvals, nBand);
		break;
	case TriSamplingUnit:
		ret = IntegrateTrisSampling(al, shvals, false);
		break;
	case TriSamplingWS:
		ret = IntegrateTrisSampling(al, shvals, true);
		break;
	case TangentPlane:
		ret = prect.IntegrateRandom(numSamples, shvals, nBand);
		break;
	case TangentPlaneUnit:
		ret = uprect.IntegrateRandom(numSamples, shvals, nBand);
		break;
	case ArvoMoments:
		ret = IntegrateArvoMoments(P, shvals);
		break;
	case LTCAnalytic:
		ret = IntegrateTrisLTC(al, shvals);
		break;
	default:
		ret = 0.f;
		LogErr("Unknown Tri integration method.");
		break;
	}

	return ret;
}

f32 SHInt::IntegrateLight(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	f32 ret;

	Rectangle arect = AreaLight::GetRectangle(al);

	// Set the visualisation vertices
	for (u32 i = 0; i < 2; ++i) {
		integrationSurface[i*6+0] = arect.p0;
		integrationSurface[i*6+1] = arect.p1;
		integrationSurface[i*6+2] = arect.p2;
		integrationSurface[i*6+3] = arect.p0;
		integrationSurface[i*6+4] = arect.p2;
		integrationSurface[i*6+5] = arect.p3;
	}

	switch (integrationMethod) {
	case UniformRandom:
		ret = arect.IntegrateRandom(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case AngularStratification:
		ret = arect.IntegrateAngularStratification(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case SphericalRectangles:
	{
		SphericalRectangle sphRect;
		sphRect.Init(arect, integrationPos);
		ret = sphRect.Integrate(integrationNrm, numSamples, shvals, nBand);
	}
		break;
	case TriSamplingUnit:
		ret = IntegrateTrisSampling(al, shvals, false);
		break;
	case TriSamplingWS:
		ret = IntegrateTrisSampling(al, shvals, true);
		break;
	case TangentPlane:
	{
		PlanarRectangle prect;
		prect.InitBary(arect, integrationPos);

		for (u32 i = 0; i < 2; ++i) {
			integrationSurface[i * 6 + 0] = prect.p0 + integrationPos;
			integrationSurface[i * 6 + 1] = prect.p1 + integrationPos;
			integrationSurface[i * 6 + 2] = prect.p2 + integrationPos;
			integrationSurface[i * 6 + 3] = prect.p0 + integrationPos;
			integrationSurface[i * 6 + 4] = prect.p2 + integrationPos;
			integrationSurface[i * 6 + 5] = prect.p3 + integrationPos;
		}

		ret = prect.IntegrateRandom(numSamples, shvals, nBand);
	}
	break;
	case TangentPlaneUnit:
	{
		PlanarRectangle prect;
		prect.InitUnit(arect, integrationPos);
		for (u32 i = 0; i < 2; ++i) {
			integrationSurface[i * 6 + 0] = prect.p0 + integrationPos;
			integrationSurface[i * 6 + 1] = prect.p1 + integrationPos;
			integrationSurface[i * 6 + 2] = prect.p2 + integrationPos;
			integrationSurface[i * 6 + 3] = prect.p0 + integrationPos;
			integrationSurface[i * 6 + 4] = prect.p2 + integrationPos;
			integrationSurface[i * 6 + 5] = prect.p3 + integrationPos;
		}
		ret = prect.IntegrateRandom(numSamples, shvals, nBand);
	}
	break;
	case ArvoMoments: 
	{
		std::vector<vec3f> verts(4);
		AreaLight::GetVertices(al, &verts[0]);
		for (int i = 0; i < 4; ++i) {
			verts[i] -= integrationPos;
			verts[i].Normalize();
		}
		Polygon P(verts);
		ret = IntegrateArvoMoments(P, shvals);
	}
		break;	
	case LTCAnalytic:
		ret = IntegrateTrisLTC(al, shvals);
		break;
	default:
		ret = 0.f;
		LogErr("Unknown Tri integration method.");
		break;
	}

	return ret;
}

void SHInt::IntegrateAreaLights() {
	if (!initialized) return;

	std::vector<float> shvals(nCoeff);
	std::vector<float> shtmp(nCoeff);
	std::fill_n(shvals.begin(), nCoeff, 0.f);

	f32 wtSum = 0.f;

	const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[0]);

	if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {
		const u32 dirCount = 2 * nBand + 1;
		const f32 norm = 1.f / (f32) dirCount;
		const u32 order = 2 * nBand + 1;

#if 1
		for (u32 i = 0; i < areaLights.size(); ++i) {
			const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[i]);

			if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {
				std::fill_n(shtmp.begin(), nCoeff, 0.f);

				f32 wt = IntegrateLight(*al, shtmp);
				wtSum += wt;

				// update SH
				for (u32 j = 0; j < nCoeff; ++j) {
					shvals[j] += shtmp[j];
				}
			}
		}

		// Reduce
		for (u32 j = 0; j < nCoeff; ++j) {
			shvals[j] *= wtSum / (f32) areaLights.size();
		}

#elif
		std::vector<vec3f> dirs;
		Sampling::SampleSphereRandom(dirs, dirCount);

		for (u32 i = 0; i < dirCount; ++i) {
			std::fill_n(shtmp.begin(), nCoeff, 0.f);
			SHEval(nBand, dirs[i].x, dirs[i].z, dirs[i].y, &shtmp[0]);

			f32 wt = 0.f;
			wt += p1.AxialMoment(dirs[i], order);
			wt += p2.AxialMoment(dirs[i], order);

			for (u32 j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j] * norm * wt;
			}
		}


#elif // testing random direction
		std::vector<vec3f> dirs;
		Sampling::SampleSphereRandom(dirs, dirCount);

		for (u32 i = 0; i < dirCount; ++i) {
			std::fill_n(shtmp.begin(), nCoeff, 0.f);
			SHEval(nBand, dirs[i].x, dirs[i].z, dirs[i].y, &shtmp[0]);

			for (u32 j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j] * norm * Sampling::SampleSphereRandomPDF();
			}
		}

#elif
#endif
	} else {
		std::memset(&integrationSurface[0], 0, 12 * sizeof(vec3f)); // remove the integration surface visu if the light isnt visible
	}
	// Update SH Coeffs
	UpdateData(shvals);
}