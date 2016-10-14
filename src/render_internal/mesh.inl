namespace Render
{
	namespace Mesh
	{
		vec3f CalcTangent( const vec3f &e1, const vec3f &e2, const vec2f &du, const vec2f &dv )
		{
			f32 den = 1.f / ( du.x * dv.y - dv.x * du.y );

			vec3f tangent = Normalize( ( e1 * dv.y - e2 * du.y ) * den );

			return tangent;
		}

		vec3f CalcBitangent( const vec3f &e1, const vec3f &e2, const vec2f &du, const vec2f &dv )
		{
			f32 den = 1.f / ( du.x * dv.y - dv.x * du.y );

			vec3f bitangent = Normalize( ( e2 * du.x - e1 * dv.x ) * den );

			return bitangent;
		}

		static void CalcTangentSpace( vec3f *vtan, vec3f *vbit, u32 indices_n, u32 vertices_n, const vec3f *vp, const vec2f *vt,
			const vec3f *vn, const u32 *idx )
		{
			const u32 triangle_n = indices_n / 3;

			for ( u32 i = 0; i < triangle_n; ++i )
			{
				for ( u32 j = 0; j < 1; ++j )
				{
					const u32 i1 = idx[i * 3];
					const u32 i2 = idx[i * 3 + 1];
					const u32 i3 = idx[i * 3 + 2];

					const vec3f &p1 = vp[i1];
					const vec3f &p2 = vp[i2];
					const vec3f &p3 = vp[i3];
					const vec2f &uv1 = vt[i1];
					const vec2f &uv2 = vt[i2];
					const vec2f &uv3 = vt[i3];

					const vec3f e1 = p2 - p1;
					const vec3f e2 = p3 - p1;
					const vec2f du = uv2 - uv1;
					const vec2f dv = uv3 - uv1;

					vec3f tangent = CalcTangent( e1, e2, du, dv );
					vec3f bitangent = CalcBitangent( e1, e2, du, dv );

					vtan[i1] = tangent;
					vtan[i2] = tangent;
					vtan[i3] = tangent;

					vbit[i1] = bitangent;
					vbit[i2] = bitangent;
					vbit[i3] = bitangent;
				}
			}
			// #if 0
			for ( u32 i = 0; i < vertices_n; ++i )
			{
				const vec3f &n = vn[i];
				const vec3f &t = vtan[i];
				const vec3f &b = vbit[i];

				vec3f tangent = Normalize( ( t - n * Dot( n, t ) ) );

				// handedness
				vec3f nCrossT = Cross( n, tangent );
				float handedness = Dot( nCrossT, b ) < 0.f ? -1.f : 1.f;
				tangent *= -1.f;

				vec3f binormal = Normalize( nCrossT * handedness );

				vtan[i] = tangent;
				vbit[i] = binormal;
			}
			// #endif
		}


		void ComputeBoundingSphere( f32 *vp, u32 vcount, vec3f *center, float *radius )
		{
			vec3f c( 0 );
			vec3f *verts = (vec3f*) vp;

			for ( u32 i = 0; i < vcount; ++i )
			{
				c += verts[i];
			}
			*center = c / (float) vcount;

			*radius = 0.f;
			for ( u32 i = 0; i < vcount; ++i )
			{
				const vec3f dV = verts[i] - *center;
				const f32 len = Len( dV );

				if ( len > *radius )
					*radius = len;
			}
		}

		Handle Build( const Desc &desc )
		{
			f32 *vp = nullptr, *vn = nullptr, *vt = nullptr, *vc = nullptr, *vtan = nullptr, *vbit = nullptr;
			u32 *idx = nullptr;

			int free_index;
			bool found_resource = FindResource( renderer->mesh_resources, desc.name, free_index );

			if ( found_resource )
			{
				return free_index;
			}

			bool deallocTangents = false;

			_internal::Data mesh;

			if ( !desc.empty_mesh )
			{
				if ( desc.indices_n > 0 && desc.indices && desc.vertices_n > 0 && desc.positions )
				{
					vp = desc.positions;
					idx = desc.indices;

					if ( desc.texcoords )
						vt = desc.texcoords;
					if ( desc.colors )
						vc = desc.colors;

					if ( desc.normals )
					{
						vn = desc.normals;
					}
					if ( desc.tangents )
					{
						vtan = desc.tangents;
						vbit = desc.bitangents;
					}
					else if ( desc.normals && desc.texcoords )
					{
						// calculate them
						deallocTangents = true;
						vtan = new f32[3 * desc.vertices_n];
						vbit = new f32[3 * desc.vertices_n];
						CalcTangentSpace( (vec3f*) vtan, (vec3f*) vbit, desc.indices_n, desc.vertices_n,
							(vec3f*) desc.positions, (vec2f*) desc.texcoords, (vec3f*) desc.normals, idx );
					}
				}
				else
				{
					// TODO : Here we allow mesh creation without data
					// It is used for TextMeshes VAO, so for now, no error
					LogErr( "Tried to create a Mesh without giving indices or positions array." );
					return -1;
				}
			}


			mesh.vertices_n = desc.vertices_n;
			mesh.indices_n = desc.indices_n;

			glGenVertexArrays( 1, &mesh.vao );
			// Disallow 0-Vao. If given VAO with index 0, ask for another one
			// this should never happen because VAO-0 is already constructed for the text
			if ( !mesh.vao ) glGenVertexArrays( 1, &mesh.vao );
			glBindVertexArray( mesh.vao );

			if ( vp )
			{
				mesh.attrib_flags = MESH_POSITIONS;
				glEnableVertexAttribArray( 0 );

				// fill position buffer
				glGenBuffers( 1, &mesh.vbo[0] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[0] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec3f ),
					vp, GL_STATIC_DRAW );

				// add it to the vao
				glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			// Check for indices
			if ( idx )
			{
				mesh.attrib_flags |= MESH_INDICES;
				glGenBuffers( 1, &mesh.ibo );
				glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.ibo );
				glBufferData( GL_ELEMENT_ARRAY_BUFFER, mesh.indices_n * sizeof( u32 ),
					idx, GL_STATIC_DRAW );
			}

			// check for normals
			if ( vn )
			{
				mesh.attrib_flags |= MESH_NORMALS;
				glEnableVertexAttribArray( 1 );

				// fill normal buffer
				glGenBuffers( 1, &mesh.vbo[1] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[1] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec3f ),
					vn, GL_STATIC_DRAW );

				// add it to vao
				glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			// check for texcoords
			if ( vt )
			{
				mesh.attrib_flags |= MESH_TEXCOORDS;
				glEnableVertexAttribArray( 2 );

				// fill normal buffer
				glGenBuffers( 1, &mesh.vbo[2] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[2] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec2f ),
					vt, GL_STATIC_DRAW );

				// add it to vao
				glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			// check for tangents, should exist if normals exist
			if ( vtan )
			{
				if ( !vn )
				{
					LogErr( "Found tangent array but no normal array." );
					return -1;
				}

				mesh.attrib_flags |= MESH_TANGENT;
				glEnableVertexAttribArray( 3 );
				glGenBuffers( 1, &mesh.vbo[3] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[3] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec3f ),
					vtan, GL_STATIC_DRAW );

				glVertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			// check for tangents, should exist if normals exist
			if ( vbit )
			{
				if ( !vn )
				{
					LogErr( "Found binormal array but no normal or tangent array." );
					return -1;
				}

				mesh.attrib_flags |= MESH_BINORMAL;
				glEnableVertexAttribArray( 4 );
				glGenBuffers( 1, &mesh.vbo[4] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[4] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec3f ),
					vbit, GL_STATIC_DRAW );

				glVertexAttribPointer( 4, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			// check for colors
			if ( vc )
			{
				mesh.attrib_flags |= MESH_COLORS;
				glEnableVertexAttribArray( 5 );

				// fill normal buffer
				glGenBuffers( 1, &mesh.vbo[5] );
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo[5] );
				glBufferData( GL_ARRAY_BUFFER, mesh.vertices_n * sizeof( vec4f ),
					vc, GL_STATIC_DRAW );

				// add it to vao
				glVertexAttribPointer( 5, 4, GL_FLOAT, GL_FALSE, 0, (GLubyte*) NULL );
			}

			if ( deallocTangents )
			{
				delete[] vtan;
				delete[] vbit;
			}

			glBindVertexArray( 0 );

			// Compute BoundingSphere and store attributes
			ComputeBoundingSphere( vp, mesh.vertices_n, &mesh.center, &mesh.radius );

			int mesh_i = (int) renderer->meshes.size();
			renderer->meshes.push_back( mesh );

			// Add created mesh to renderer resources
			AddResource( renderer->mesh_resources, free_index, desc.name, mesh_i );

			return mesh_i;
		}

		void Destroy( Handle h )
		{
			if ( Exists( h ) )
			{
				_internal::Data &mesh = renderer->meshes[h];
				glDeleteBuffers( 6, mesh.vbo );
				glDeleteBuffers( 1, &mesh.ibo );
				glDeleteVertexArrays( 1, &mesh.vao );
				mesh.attrib_flags = MESH_POSITIONS;
				mesh.vertices_n = 0;
				mesh.indices_n = 0;


				// Remove it as a loaded resource
				for ( u32 i = 0; i < renderer->mesh_resources.size(); ++i )
				{
					if ( (int) renderer->mesh_resources[i].handle == h )
					{
						renderer->mesh_resources[i].handle = -1;
						renderer->mesh_resources[i].name = "";
					}
				}
			}
		}

		void Bind( Handle h )
		{
			GLint vao = Exists( h ) ? h : -1;

			if ( renderer->curr_GL_vao != vao )
			{
				renderer->curr_GL_vao = vao;
				glBindVertexArray( vao >= 0 ? (int) renderer->meshes[vao].vao : -1 );
			}
		}

		void Render( Handle h )
		{
			if ( Exists( h ) )
			{
				const _internal::Data &md = renderer->meshes[h];

				Bind( h );
				glDrawElements( GL_TRIANGLES, md.indices_n, GL_UNSIGNED_INT, 0 );
			}
			//if (state && state.type > ANIM_NONE) {
			//}
			//else {
			// identity bone matrix for meshes having no mesh animation
			//mat4 identity;
			//mat4_identity(identity);
			//shader_send_mat4(UNIFORM_BONEMATRIX0, identity);
			//}
		}


		bool Exists( Handle h )
		{
			return h >= 0 && h < (int) renderer->meshes.size() && renderer->meshes[h].vao > 0;
		}

		bool Exists( const std::string &resourceName, Handle &res )
		{
			return FindResource( renderer->mesh_resources, resourceName, res, false );
		}
		/*
			void SetAnimation(Handle h, AnimState &state, AnimType type) {
				if (Exists(h)) {
					state.curr_frame = 0;
					state.curr_time = 0.f;
					state.type = type;

					if (type > ANIM_NONE) {
						state.duration = renderer->meshes[h].animations[type - 1].duration;
						UpdateAnimation(h, state, 0.f);
					}
				}
			}

			void UpdateAnimation(Handle h, AnimState &state, float dt) {
				mat4f root_mat;
				root_mat.Identity();
				root_mat = root_mat.RotateX(-M_PI_OVER_TWO);	// rotate root by 90X for left-handedness

				if (Exists(h)) {
					const _Animation &anim = renderer->meshes[h].animations[state.type - 1];
					state.curr_time += dt;

					if (anim.used && state.curr_time > anim.frame_duration[state.curr_frame + 1]) {
						++state.curr_frame;
						if (state.curr_frame == anim.frame_n - 1) {
							state.curr_frame = 0;
							state.curr_time = 0.f;
						}
					}
				}
			}
		*/
		Handle BuildSphere()
		{
			// Test resource existence before recreating it
			{
				Handle h;
				if ( Exists( "Sphere1", h ) )
				{
					return h;
				}
			}

			const f32 radius = 1.f;
			const u32 nLon = 32, nLat = 24;

			const u32 nVerts = ( nLon + 1 ) * nLat + 2;
			// const u32 nTriangles = nVerts * 2;
			const u32 nIndices = ( nLat - 1 )*nLon * 6 + nLon * 2 * 3;//nTriangles * 3;

																	// Positions
			vec3f pos[nVerts];
			pos[0] = vec3f( 0, 1, 0 ) * radius;
			for ( u32 lat = 0; lat < nLat; ++lat )
			{
				f32 a1 = M_PI * (f32) ( lat + 1 ) / ( nLat + 1 );
				f32 sin1 = std::sin( a1 );
				f32 cos1 = std::cos( a1 );

				for ( u32 lon = 0; lon <= nLon; ++lon )
				{
					f32 a2 = M_TWO_PI * (f32) ( lon == nLon ? 0 : lon ) / nLon;
					f32 sin2 = std::sin( a2 );
					f32 cos2 = std::cos( a2 );

					pos[lon + lat * ( nLon + 1 ) + 1] = vec3f( sin1 * cos2, cos1, sin1 * sin2 ) * radius;
				}
			}
			pos[nVerts - 1] = vec3f( 0, 1, 0 ) * -radius;

			// Normals
			vec3f nrm[nVerts];
			for ( u32 i = 0; i < nVerts; ++i )
			{
				nrm[i] = Normalize( pos[i] );
			}

			// UVs
			vec2f uv[nVerts];
			uv[0] = vec2f( 0, 1 );
			uv[nVerts - 1] = vec2f( 0.f );
			for ( u32 lat = 0; lat < nLat; ++lat )
			{
				for ( u32 lon = 0; lon <= nLon; ++lon )
				{
					uv[lon + lat * ( nLon + 1 ) + 1] = vec2f( lon / (f32) nLon, 1.f - ( lat + 1 ) / (f32) ( nLat + 1 ) );
				}
			}

			// Triangles/Indices
			u32 indices[nIndices];
			{
				// top
				u32 i = 0;
				for ( u32 lon = 0; lon < nLon; ++lon )
				{
					indices[i++] = lon + 2;
					indices[i++] = lon + 1;
					indices[i++] = 0;
				}

				// middle
				for ( u32 lat = 0; lat < nLat - 1; ++lat )
				{
					for ( u32 lon = 0; lon < nLon; ++lon )
					{
						u32 curr = lon + lat * ( nLon + 1 ) + 1;
						u32 next = curr + nLon + 1;

						indices[i++] = curr;
						indices[i++] = curr + 1;
						indices[i++] = next + 1;

						indices[i++] = curr;
						indices[i++] = next + 1;
						indices[i++] = next;
					}
				}

				// bottom
				for ( u32 lon = 0; lon < nLon; ++lon )
				{
					indices[i++] = nVerts - 1;
					indices[i++] = nVerts - ( lon + 2 ) - 1;
					indices[i++] = nVerts - ( lon + 1 ) - 1;
				}
			}

			Desc desc( "Sphere1", false, nIndices, indices, nVerts, (f32*) pos, (f32*) nrm, (f32*) uv );
			return Build( desc );
		}

		Handle BuildBox()
		{
			// Test resource existence before recreating it
			{
				Handle h;
				if ( Exists( "Box1", h ) )
				{
					return h;
				}
			}

			vec3f pos[24] = {
				vec3f( -1, -1, -1 ),
				vec3f( -1, -1, 1 ),
				vec3f( -1, 1, 1 ),
				vec3f( -1, 1, -1 ),

				vec3f( 1, -1, 1 ),
				vec3f( 1, -1, -1 ),
				vec3f( 1, 1, -1 ),
				vec3f( 1, 1, 1 ),

				vec3f( -1, -1, 1 ),
				vec3f( -1, -1, -1 ),
				vec3f( 1, -1, -1 ),
				vec3f( 1, -1, 1 ),

				vec3f( -1, 1, -1 ),
				vec3f( -1, 1, 1 ),
				vec3f( 1, 1, 1 ),
				vec3f( 1, 1, -1 ),

				vec3f( 1, 1, -1 ),
				vec3f( 1, -1, -1 ),
				vec3f( -1, -1, -1 ),
				vec3f( -1, 1, -1 ),

				vec3f( -1, 1, 1 ),
				vec3f( -1, -1, 1 ),
				vec3f( 1, -1, 1 ),
				vec3f( 1, 1, 1 )
			};

			vec3f nrm[24] = {
				vec3f( -1, 0, 0 ),
				vec3f( -1, 0, 0 ),
				vec3f( -1, 0, 0 ),
				vec3f( -1, 0, 0 ),

				vec3f( 1, 0, 0 ),
				vec3f( 1, 0, 0 ),
				vec3f( 1, 0, 0 ),
				vec3f( 1, 0, 0 ),

				vec3f( 0, -1, 0 ),
				vec3f( 0, -1, 0 ),
				vec3f( 0, -1, 0 ),
				vec3f( 0, -1, 0 ),

				vec3f( 0, 1, 0 ),
				vec3f( 0, 1, 0 ),
				vec3f( 0, 1, 0 ),
				vec3f( 0, 1, 0 ),

				vec3f( 0, 0, -1 ),
				vec3f( 0, 0, -1 ),
				vec3f( 0, 0, -1 ),
				vec3f( 0, 0, -1 ),

				vec3f( 0, 0, 1 ),
				vec3f( 0, 0, 1 ),
				vec3f( 0, 0, 1 ),
				vec3f( 0, 0, 1 )
			};

			vec2f uv[24] = {
				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),

				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),

				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),

				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),

				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),

				vec2f( 0, 1 ),
				vec2f( 0, 0 ),
				vec2f( 1, 0 ),
				vec2f( 1, 1 ),
			};

			u32 indices[36];
			for ( u32 i = 0; i < 6; ++i )
			{
				indices[i * 6 + 0] = i * 4 + 0;
				indices[i * 6 + 1] = i * 4 + 1;
				indices[i * 6 + 2] = i * 4 + 2;

				indices[i * 6 + 3] = i * 4 + 0;
				indices[i * 6 + 4] = i * 4 + 2;
				indices[i * 6 + 5] = i * 4 + 3;
			}

			Desc desc( "Box1", false, 36, indices, 24, (f32*) pos, (f32*) nrm, (f32*) uv );
			return Build( desc );
		}

		Handle BuildSHVisualization( const float *shCoeffs, const u32 bandN, const std::string &meshName, bool shNormalization, const u32 numPhi, const u32 numTheta )
		{
			const u32 faceCount = numPhi * 2 + ( numTheta - 3 ) * numPhi * 2;
			const u32 indexCount = faceCount * 3;
			const u32 vertexCount = 2 + numPhi * ( numTheta - 2 );
			const u32 shCoeffsN = bandN * bandN;

			std::vector<u32> indices( indexCount );
			std::vector<vec3f> pos( vertexCount );
			std::vector<vec3f> nrm( vertexCount );

			u32 fi, vi, rvi = 0, ci;

			// Triangles
			{
				u32 rfi = 0;

				// top cap
				for ( fi = 0; fi < numPhi; ++fi )
				{
					indices[fi * 3 + 0] = 0;
					indices[fi * 3 + 1] = fi + 1;
					indices[fi * 3 + 2] = ( ( fi + 1 ) % numPhi ) + 1;
					++rfi;
				}

				// spans
				for ( vi = 1; vi < numTheta - 2; ++vi )
				{
					for ( fi = 0; fi < numPhi; ++fi )
					{
						indices[rfi * 3 + 0] = ( 1 + ( vi - 1 ) * numPhi + fi );
						indices[rfi * 3 + 1] = ( 1 + (vi) * numPhi + fi );
						indices[rfi * 3 + 2] = ( 1 + (vi) * numPhi + ( ( fi + 1 ) % numPhi ) );
						++rfi;
						indices[rfi * 3 + 0] = ( 1 + ( vi - 1 ) * numPhi + fi );
						indices[rfi * 3 + 1] = ( 1 + (vi) * numPhi + ( ( fi + 1 ) % numPhi ) );
						indices[rfi * 3 + 2] = ( 1 + ( vi - 1 ) * numPhi + ( ( fi + 1 ) % numPhi ) );
						++rfi;
					}
				}

				// bottom cap
				for ( fi = 0; fi < numPhi; ++fi )
				{
					indices[rfi * 3 + 0] = ( 1 + numPhi * ( numTheta - 3 ) + fi );
					indices[rfi * 3 + 1] = ( 1 + numPhi * ( numTheta - 2 ) );
					indices[rfi * 3 + 2] = ( 1 + numPhi * ( numTheta - 3 ) + ( ( fi + 1 ) % numPhi ) );
					++rfi;
				}
			}

			// Scale function coeffs
			std::vector<float> scaledCoeffs( shCoeffsN );
			float normFactor = shNormalization ? 0.3334f / shCoeffs[0] : 1.f;
			for ( ci = 0; ci < shCoeffsN; ++ci )
			{
				scaledCoeffs[ci] = shCoeffs[ci] * normFactor;
			}

			std::vector<float> shVals( shCoeffsN );

			for ( vi = 0; vi < numTheta; ++vi )
			{
				float theta = vi * M_PI / ( (float) numTheta - 1.f );
				float cT = std::cos( theta );
				float sT = std::sin( theta );

				if ( vi && ( vi < numTheta - 1 ) )
				{ // spans
					for ( fi = 0; fi < numPhi; ++fi )
					{
						float phi = fi * 2.f * M_PI / (float) numPhi;
						float cP = std::cos( phi );
						float sP = std::sin( phi );

						vec3f vpos( sT * cP, sT * sP, cT );
						vec3f vnrm = Normalize( vpos );

						// Get sh coeffs for this direction
						std::fill_n( shVals.begin(), shCoeffsN, 0.f );
						SHEval( bandN, vpos.x, vpos.z, vpos.y, &shVals[0] );

						float fVal = 0.f;
						for ( ci = 0; ci < shCoeffsN; ++ci )
						{
							fVal += shVals[ci] * scaledCoeffs[ci];
						}

						if ( fVal < 0.f )
						{
							fVal *= -1.f;
						}

						vpos *= fVal;

						pos[rvi] = vpos;
						nrm[rvi] = vnrm;
						++rvi;
					}
				}
				else
				{ // Cap 2 points
					if ( vi ) theta -= 1e-4f;
					else theta = 1e-4f;

					cT = std::cos( theta );
					sT = std::sin( theta );
					float cP, sP;
					cP = sP = 0.707106769f; // sqrt(0.5)

					vec3f vpos( sT * cP, sT * sP, cT );
					vec3f vnrm = Normalize( vpos );

					// Get sh coeffs for this direction
					std::fill_n( shVals.begin(), shCoeffsN, 0.f );
					SHEval( bandN, vpos.x, vpos.z, vpos.y, &shVals[0] );

					float fVal = 0.f;
					for ( ci = 0; ci < shCoeffsN; ++ci )
					{
						fVal += shVals[ci] * scaledCoeffs[ci];
					}

					if ( fVal < 0.f )
					{
						fVal *= -1.f;
					}

					vpos *= fVal;

					pos[rvi] = vpos;
					nrm[rvi] = vnrm;
					++rvi;
				}
			}

			Desc desc( meshName, false, indexCount, (u32*) ( &indices[0] ), vertexCount, (f32*) ( &pos[0] ), (f32*) ( &nrm[0] ) );
			Handle h = Build( desc );

			return h;
		}
	}



	namespace TextMesh
	{
		Handle Build( Desc &desc )
		{
			// Create a new textmesh with the
			Handle tmesh_handle = SetString( -1, desc.font_handle, desc.string );
			return tmesh_handle;
		}

		Handle SetString( Handle h, Font::Handle fh, const std::string &str )
		{
			if ( !Font::Exists( fh ) )
			{
				LogErr( "Font handle ", fh, " is not a valid renderer resource." );
				return false;
			}

			bool updated = true;        // true if this is an updated of an existing text
			_internal::Data *dst;// = renderer->textmeshes[0];	// temporary reference

			if ( h >= 0 )
			{
				Assert( h < (int) renderer->textmeshes.size() );
				dst = &renderer->textmeshes[h];
			}
			else
			{
				updated = false;
				renderer->textmeshes.push_back( _internal::Data() );
				dst = &renderer->textmeshes.back();
				*dst = _internal::Data();
			}

			if ( !_internal::CreateFromString( *dst, renderer->fonts[fh], str ) )
			{
				LogErr( "Error creating TextMesh from given string." );
				return -1;
			}

			return updated ? h : (Handle) ( renderer->textmeshes.size() - 1 );
		}

		bool Exists( Handle h )
		{
			return h >= 0 && h < (int) renderer->textmeshes.size() && renderer->textmeshes[h].vbo > 0;
		}

		void Destroy( Handle h )
		{
			if ( Exists( h ) )
			{
				_internal::Data &textmesh = renderer->textmeshes[h];
				glDeleteBuffers( 1, &textmesh.vbo );
				textmesh.vbo = 0;
			}
		}

		void Bind( Handle h )
		{
			if ( Exists( h ) )
			{
				_internal::Data &textmesh = renderer->textmeshes[h];

				glBindBuffer( GL_ARRAY_BUFFER, textmesh.vbo );
				glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*) NULL );
				glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 0,
					(GLvoid*) ( textmesh.vertices_n * 2 * sizeof( f32 ) ) );
				glEnableVertexAttribArray( 0 );   // enable positions
				glDisableVertexAttribArray( 1 );  // no normals
				glEnableVertexAttribArray( 2 );   // enable texcoords
			}
		}

		void Render( Handle h )
		{
			Bind( h );
			glDrawArrays( GL_TRIANGLES, 0, renderer->textmeshes[h].vertices_n );
		}

		namespace _internal
		{
			static u32 sizes_n = 6;
			static u32 letter_count[] = { 16, 32, 128, 512, 1024, 2048 };

			bool CreateFromString( Data &mesh, const Font::_internal::Data &font, const std::string &str )
			{
				u32 str_len = (u32) str.length();

				if ( str_len > mesh.curr_letter_n )
				{
					// find upper bound for data size
					u32 lc = 0;
					for ( u32 i = 0; i < sizes_n; ++i )
					{
						if ( str_len < letter_count[i] )
						{
							lc = letter_count[i];
							break;
						}
					}

					if ( lc <= 0 )
					{
						LogErr( "Given string is too large. Max characters is ", letter_count[sizes_n - 1] );
						return false;
					}

					// check if need to expand vertex array
					if ( lc > mesh.curr_letter_n )
					{
						mesh.data.resize( 4 * 6 * lc );

						// create mesh if first time
						if ( 0 == mesh.vbo )
						{
							glGenBuffers( 1, &mesh.vbo );
						}
						glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo );
						glBufferData( GL_ARRAY_BUFFER, 4 * 6 * lc * sizeof( f32 ),
							NULL, GL_DYNAMIC_DRAW );
					}

					mesh.curr_letter_n = lc;
				}

				mesh.vertices_n = str_len * 6;

				int n_pos = 0, n_tex = mesh.vertices_n * 2;
				int fw = font.size.x, fh = font.size.y;

				f32 x_left = 0;
				f32 x = 0, y = 0;

				const Font::_internal::Glyph *glyphs = font.glyphs;

				for ( u32 i = 0; i < str_len; ++i )
				{
					int ci = (int) str[i];

					if ( 0 == str[i] ) break;  // just in case str_len is wrong

											 // TODO : Parameter for max length before line wrap
					if (/*x > 300 ||*/ '\n' == str[i] )
					{
						y += ( fh + 4 );
						x = x_left;
						n_pos += 12;
						n_tex += 12;
						continue;
					}
					else if ( 32 == str[i] )
					{
						x += ( fh / 3.f );
						//            n_pos += 12;
						//            n_tex += 12;

						for ( u32 i = 0; i < 12; ++i )
						{
							mesh.data[n_pos++] = 0.f;
							mesh.data[n_tex++] = 0.f;
						}

						continue;
					}

					f32 x2 = x + glyphs[ci].position[0];
					f32 y2 = y + ( fh - glyphs[ci].position[1] );
					f32 w = (f32) glyphs[ci].size[0];
					f32 h = (f32) glyphs[ci].size[1];

					if ( !w || !h ) continue;

					x += glyphs[ci].advance[0];
					y += glyphs[ci].advance[1];

					mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2;
					mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2 + h;
					mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2 + h;

					mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2;
					mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2 + h;
					mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2;

					//log_info("<%g,%g>, <%g,%g>", glyphs[ci].x_offset, 
					//0.f, 
					//glyphs[ci].x_offset+glyphs[ci].size[0]/(f32)fw, 
					//glyphs[ci].size[1]/(f32)fh);
					mesh.data[n_tex++] = glyphs[ci].x_offset;
					mesh.data[n_tex++] = 0;
					mesh.data[n_tex++] = glyphs[ci].x_offset;
					mesh.data[n_tex++] = (f32) glyphs[ci].size[1] / (f32) fh;
					mesh.data[n_tex++] = glyphs[ci].x_offset + (f32) glyphs[ci].size[0] / (f32) fw;
					mesh.data[n_tex++] = (f32) glyphs[ci].size[1] / (f32) fh;

					mesh.data[n_tex++] = glyphs[ci].x_offset;
					mesh.data[n_tex++] = 0;
					mesh.data[n_tex++] = glyphs[ci].x_offset + (f32) glyphs[ci].size[0] / (f32) fw;
					mesh.data[n_tex++] = (f32) glyphs[ci].size[1] / (f32) fh;
					mesh.data[n_tex++] = glyphs[ci].x_offset + (f32) glyphs[ci].size[0] / (f32) fw;
					mesh.data[n_tex++] = 0;
				}

				// update vbo
				glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo );
				glBufferSubData( GL_ARRAY_BUFFER, (GLintptr) 0, 4 * mesh.vertices_n * sizeof( f32 ),
					&mesh.data[0] );

				return true;
			}
		}
	}

	namespace SpriteSheet
	{
		Handle LoadFromFile( const std::string &filename )
		{
			Json ss_json;
			if ( !ss_json.Open( filename ) )
			{
				LogErr( "Error loading spritesheet json file." );
				return -1;
			}

			_internal::Data ss;

			ss.file_name = filename;
			ss.resource_name = Json::ReadString( ss_json.root, "name", "" );
			std::string tex_file = Json::ReadString( ss_json.root, "texture", "" );

			// try to find the resource
			int free_index;
			bool found_resource = FindResource( renderer->spritesheets_resources,
				ss.resource_name, free_index );

			if ( found_resource )
			{
				return free_index;
			}

			// try to load the texture resource or get it if it exists
			Texture::Desc tdesc( tex_file );
			ss.sheet_texture = Texture::Build( tdesc );

			if ( ss.sheet_texture == -1 )
			{
				LogErr( "Error loading texture file during spritesheet creation." );
				return -1;
			}

			cJSON *sprite_arr = cJSON_GetObjectItem( ss_json.root, "sprites" );
			if ( sprite_arr )
			{
				int sprite_count = cJSON_GetArraySize( sprite_arr );
				for ( int i = 0; i < sprite_count; ++i )
				{
					Sprite sprite;
					cJSON *sprite_root = cJSON_GetArrayItem( sprite_arr, i );
					if ( cJSON_GetArraySize( sprite_root ) != 5 )
					{
						LogErr( "A Sprite definition in a spritesheet must have 5 fields : x y w h name." );
						continue;
					}
					sprite.geometry.x = cJSON_GetArrayItem( sprite_root, 0 )->valueint;
					sprite.geometry.y = cJSON_GetArrayItem( sprite_root, 1 )->valueint;
					sprite.geometry.w = cJSON_GetArrayItem( sprite_root, 2 )->valueint;
					sprite.geometry.h = cJSON_GetArrayItem( sprite_root, 3 )->valueint;
					sprite.name = cJSON_GetArrayItem( sprite_root, 4 )->valuestring;

					ss.sprites.push_back( sprite );
				}
			}

			Handle ss_i = (Handle) renderer->spritesheets.size();
			renderer->spritesheets.push_back( ss );

			AddResource( renderer->spritesheets_resources, free_index, ss.resource_name, ss_i );

			return ss_i;
		}

		void Destroy( Handle h )
		{
			if ( h >= 0 && h < (int) renderer->spritesheets.size() )
			{
				Texture::Destroy( renderer->spritesheets[h].sheet_texture );
				renderer->spritesheets[h].sprites.clear();
			}
		}
	}
}