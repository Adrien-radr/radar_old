#include "geometry.h"
#include "common/SHEval.h"

#include <algorithm>
#include <complex>
//#pragma optimize("", off)

static bool even( int n )
{
	return !( n & 1 );
}

static int sign( f32 b )
{
	return ( b >= 0 ) - ( b < 0 );
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

vec3f Plane::RayIntersection( const vec3f &rayOrg, const vec3f &rayDir ) const
{
	const f32 distance = Dot( N, P - rayOrg ) / Dot( N, rayDir );
	return rayOrg + rayDir * distance;
}

vec3f Plane::ClampPointInRect( const Rectangle &rect, const vec3f &point ) const
{
	const vec3f dir = P - point;

	vec2f dist2D( Dot( dir, rect.ex ), Dot( dir, rect.ey ) );
	vec2f hSize( rect.hx, rect.hy );
	dist2D.x = std::min( hSize.x, std::max( -hSize.x, dist2D.x ) );
	dist2D.y = std::min( hSize.y, std::max( -hSize.y, dist2D.y ) );

	return point + rect.ex * dist2D.x + rect.ey * dist2D.y;
}
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

Polygon::Polygon( const std::vector<vec3f> &pts )
{
	int nv = (int) pts.size();
	for ( int i = 0; i < nv - 1; ++i )
	{
		edges.push_back( Edge{ pts[i], pts[i + 1] } );
	}
	edges.push_back( Edge{ pts[nv - 1], pts[0] } );
}

f32 Polygon::SolidAngle() const
{

	if ( edges.size() == 3 )
	{
		const vec3f &A = edges[0].A;
		const vec3f &B = edges[1].A;
		const vec3f &C = edges[2].A;

		// Arvo solid angle : alpha + beta + gamma - pi
		// Oosterom & Strackee 83 method
		const vec3f tmp = Cross( A, B );
		const f32 num = std::fabsf( Dot( tmp, A ) );
		const f32 r1 = std::sqrtf( Dot( A, A ) );
		const f32 r2 = std::sqrtf( Dot( B, B ) );
		const f32 r3 = std::sqrtf( Dot( C, C ) );

		const f32 denom = r1 * r2 * r3 + Dot( A, B ) * r3 + Dot( A, C ) * r2 + Dot( B, C ) * r1;

		// tan(phi/2) = num/denom
		f32 halPhi = std::atan2f( num, denom );
		if ( halPhi < 0.f ) halPhi += M_PI;

		return 2.f * halPhi;
	}
	else
	{
		// Algorithm of polyhedral cones by Mazonka http://arxiv.org/pdf/1205.1396v2.pdf
		std::complex<float> z( 1, 0 );
		for ( unsigned int k = 0; k < edges.size(); ++k )
		{
			const vec3f& A = edges[( k > 0 ) ? k - 1 : edges.size() - 1].A;
			const vec3f& B = edges[k].A;
			const vec3f& C = edges[k].B;

			const float ak = Dot( A, C );
			const float bk = Dot( A, B );
			const float ck = Dot( B, C );
			const float dk = Dot( A, Cross( B, C ) );
			const std::complex<float> zk( bk*ck - ak, dk );
			z *= zk;
		}
		const float arg = std::arg( z );
		return arg;
	}
}

f32 Polygon::CosSumIntegralArvo( f32 x, f32 y, f32 c, int nMin, int nMax ) const
{
	const f32 sinx = std::sinf( x );
	const f32 siny = std::sinf( y );

	int i = even( nMax ) ? 0 : 1;
	f32 F = even( nMax ) ? y - x : siny - sinx;
	f32 S = 0.f;

	while ( i <= nMax )
	{
		if ( i >= nMin )
			S += std::pow( c, i ) * F;

		const f32 T = std::pow( std::cosf( y ), i + 1 ) * siny - std::pow( std::cosf( x ), i + 1 ) * sinx;
		F = ( T + ( i + 1 )*F ) / ( i + 2 );
		i += 2;
	}

	return S;
}

f32 Polygon::LineIntegralArvo( const vec3f & A, const vec3f & B, const vec3f & w, int nMin, int nMax ) const
{
	const f32 eps = 1e-7f;
	if ( ( nMax < 0 ) || ( ( fabs( Dot( w, A ) ) < eps ) && ( fabs( Dot( w, B ) ) < eps ) ) )
	{
		return 0.f;
	}

	vec3f s = Normalize( A );

	const f32 sDotB = Dot( s, B );

	vec3f t = Normalize( B - s * sDotB );

	const f32 a = Dot( w, s );
	const f32 b = Dot( w, t );
	const f32 c = std::sqrtf( a*a + b*b );

	const f32 cos_l = sDotB / Dot( B, B );
	const f32 l = std::acosf( std::max( -1.f, std::min( 1.f, cos_l ) ) );
	const f32 phi = sign( b ) * std::acosf( a / c );

	return CosSumIntegralArvo( -phi, l - phi, c, nMin, nMax );
}

f32 Polygon::BoundaryIntegralArvo( const vec3f & w, const vec3f & v, int nMin, int nMax ) const
{
	f32 b = 0.f;

	for ( const Edge &e : edges )
	{
		vec3f n = Normalize( Cross( e.A, e.B ) );

		b += Dot( n, v ) * LineIntegralArvo( e.A, e.B, w, nMin, nMax );
	}

	return b;
}

f32 Polygon::AxialMomentArvo( const vec3f & w, int order )
{
	f32 a = -BoundaryIntegralArvo( w, w, 0, order - 1 );

	if ( even( order ) )
	{
		a += SolidAngle();
	}

	return a / ( order + 1 );
}

f32 Polygon::DoubleAxisMomentArvo( const vec3f & w, const vec3f & v, int order )
{
	if ( order == 0 )
		return AxialMomentArvo( w, order );

	f32 a = AxialMomentArvo( w, order - 1 );
	f32 b = BoundaryIntegralArvo( w, v, order, order );

	return ( order * a * Dot( w, v ) - b ) / ( order + 2 );
}

void Polygon::CosSumIntegral( f32 x, f32 y, f32 c, int n, std::vector<f32> &R ) const
{
	const f32 sinx = std::sinf( x );
	const f32 siny = std::sinf( y );
	const f32 cosx = std::cosf( x );
	const f32 cosy = std::cosf( y );
	const f32 cosxsq = cosx * cosx;
	const f32 cosysq = cosy * cosy;

	static const vec2f i1( 1, 1 );
	static const vec2f i2( 2, 2 );
	vec2i i( 0, 1 );
	vec2f F( y - x, siny - sinx );
	vec2f S( 0.f, 0.f );

	vec2f pow_c( 1.f, c );
	vec2f pow_cosx( cosx, cosxsq );
	vec2f pow_cosy( cosy, cosysq );

	while ( i[1] <= n )
	{
		S += pow_c * F;

		R[i[1] + 0] = S[0];
		R[i[1] + 1] = S[1];

		vec2f T = pow_cosy * siny - pow_cosx * sinx;
		F = ( T + ( i + i1 ) * F ) / ( i + i2 );

		i += i2;
		pow_c *= c*c;
		pow_cosx *= cosxsq;
		pow_cosy *= cosysq;
	}
}

void Polygon::LineIntegral( const vec3f &A, const vec3f &B, const vec3f &w, int n, std::vector<f32> &R ) const
{
	const f32 eps = 1e-7f;
	if ( ( n < 0 ) || ( ( fabs( Dot( w, A ) ) < eps ) && ( fabs( Dot( w, B ) ) < eps ) ) )
	{
		return;
	}

	vec3f s = Normalize( A );

	const f32 sDotB = Dot( s, B );

	vec3f t = Normalize( B - s * sDotB );

	const f32 a = Dot( w, s );
	const f32 b = Dot( w, t );
	const f32 c = std::sqrtf( a*a + b*b );

	const f32 cos_l = sDotB / Dot( B, B );
	const f32 l = std::acosf( std::max( -1.f, std::min( 1.f, cos_l ) ) );
	const f32 phi = sign( b ) * std::acosf( a / c );

	CosSumIntegral( -phi, l - phi, c, n, R );
}

void Polygon::BoundaryIntegral( const vec3f &w, const vec3f &v, int n, std::vector<f32> &R ) const
{
	std::vector<f32> b( n + 2, 0.f );

	for ( const Edge &e : edges )
	{
		vec3f nrm = Normalize( Cross( e.A, e.B ) );
		f32 nDotv = Dot( nrm, v );

		LineIntegral( e.A, e.B, w, n, b );

		for ( int i = 0; i < n + 2; ++i )
			R[i] += b[i] * nDotv;
	}
}

void Polygon::AxialMoment( const vec3f &w, int order, std::vector<f32> &R ) const
{
	// Compute the Boundary Integral of the polygon
	BoundaryIntegral( w, w, order, R );

	// - boundary + solidangle for even orders
	f32 sA = SolidAngle();

	for ( u32 i = 0; i < R.size(); ++i )
	{
		R[i] *= -1.f; // - boundary

		// add the solid angle for even orders
		if ( even( i ) )
		{
			R[i] += sA;
		}

		// normalize by order+1
		R[i] *= -1.f / (f32) ( i + 1 );
	}
}

std::vector<f32> Polygon::AxialMoments( const std::vector<vec3f> &directions ) const
{
	const u32 dsize = (u32) directions.size();
	const u32 order = ( dsize - 1 ) / 2 + 1;

	std::vector<f32> result( dsize * order, 0.f );

	for ( u32 i = 0; i < dsize; ++i )
	{
		std::vector<f32> dirResult( order, 0.f );
		const vec3f &d = directions[i];
		AxialMoment( d, order - 1, dirResult );

		for ( u32 j = 0; j < order; ++j )
		{
			result[i*order + j] = dirResult[j];
		}
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

void Triangle::InitUnit( const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos )
{
	q0 = Normalize( p0 - intPos );
	q1 = Normalize( p1 - intPos );
	q2 = Normalize( p2 - intPos );

	const vec3f d1 = q1 - q0;
	const vec3f d2 = q2 - q0;

	const vec3f nrm = -Cross( d1, d2 );
	const f32 nrmLen = std::sqrtf( Dot( nrm, nrm ) );
	area = solidAngle = nrmLen * 0.5f;

	// compute inset triangle's unit normal
	const f32 areaThresh = 1e-5f;
	bool badPlane = -1.0f * Dot( q0, nrm ) <= 0.0f;

	if ( badPlane || area < areaThresh )
	{
		unitNormal = Normalize( -( q0 + q1 + q2 ) );
	}
	else
	{
		unitNormal = nrm / nrmLen;
	}
}

/// Same as InitUnit, except the resulting triangle is kept in world space. 
/// triNormal is the normal of the plane collinear with the triangle
void Triangle::InitWS( const vec3f &triNormal, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos )
{
	unitNormal = triNormal;

	q0 = p0 - intPos;
	q1 = p1 - intPos;
	q2 = p2 - intPos;

	ComputeArea();

	const vec3f bary = ( q0 + q1 + q2 ) * 0.3333333333333f;
	const f32 rayLenSqr = Dot( bary, bary );
	const f32 rayLen = std::sqrtf( rayLenSqr );
	solidAngle = -Dot( bary, unitNormal ) * ( area / ( rayLenSqr * rayLen ) );
}

u32 Triangle::Subdivide4( Triangle subdivided[4] ) const
{
	vec3f q01 = ( q0 + q1 );
	vec3f q02 = ( q0 + q2 );
	vec3f q12 = ( q1 + q2 );

	subdivided[0].InitUnit( q0, q01, q02, vec3f( 0.f ) );
	subdivided[1].InitUnit( q01, q1, q12, vec3f( 0.f ) );
	subdivided[2].InitUnit( q02, q12, q2, vec3f( 0.f ) );
	subdivided[3].InitUnit( q01, q12, q02, vec3f( 0.f ) );

	return 4;
}

void Triangle::ComputeArea()
{
	const vec3f v1 = q1 - q0, v2 = q2 - q0;
	const vec3f n1 = Cross( v1, v2 );

	area = std::fabsf( Dot( n1, unitNormal ) ) * 0.5f;
}

vec3f Triangle::SamplePoint( f32 u1, f32 u2 ) const
{
	const f32 su1 = std::sqrtf( u1 );
	const vec2f bary( 1.f - su1, u2 * su1 );

	return q0 * bary.x + q1 * bary.y + q2 * ( 1.f - bary.x - bary.y );
}

vec3f Triangle::SamplePointBary( f32 baryX, f32 baryY ) const
{
	return q0 * baryX + q1 * baryY + q2 * ( 1.f - baryX - baryY );
}

f32 Triangle::SampleDir( vec3f &rayDir, const f32 s, const f32 t ) const
{
	return SampleDir( rayDir, SamplePoint( s, t ) );
	/*const f32 rayLenSqr = rayDir.Dot(rayDir);
	const f32 rayLen = std::sqrtf(rayLenSqr);
	rayDir /= rayLen;

	const f32 costheta = -unitNormal.Dot(rayDir);

	return costheta / rayLenSqr; */
}

f32 Triangle::SampleDir( vec3f &rayDir, const vec3f &pt ) const
{
	rayDir = pt;
	const f32 rayLenSqr = Dot( rayDir, rayDir );
	const f32 rayLen = std::sqrtf( rayLenSqr );
	rayDir /= rayLen;

	const f32 costheta = -Dot( unitNormal, rayDir );

	return costheta / rayLenSqr;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

Rectangle::Rectangle( const std::vector<vec3f>& verts )
{
	Assert( verts.size() == 4 );
	p0 = verts[0];
	p1 = verts[1];
	p2 = verts[2];
	p3 = verts[3];

	position = ( p0 + p1 + p2 + p3 ) / 4.f;
	ex = p1 - p0;
	hx = 0.5f * Len( ex );
	ex = Normalize( ex );
	ey = p3 - p0;
	hy = 0.5f * Len( ey );
	ey = Normalize( ey );

	ez = Normalize( -Cross( ex, ey ) );
}

Rectangle::Rectangle( const Polygon & P )
{
	Assert( P.edges.size() == 4 );

	std::vector<vec3f> verts( 4 );

	verts[0] = P.edges[0].A;
	verts[1] = P.edges[1].A;
	verts[2] = P.edges[2].A;
	verts[3] = P.edges[3].A;

	*this = Rectangle( verts );
}

vec3f Rectangle::SamplePoint( f32 u1, f32 u2 ) const
{
	const vec3f bl = position - ex * hx - ey * hy;
	const f32 w = hx * 2.f;
	const f32 h = hy * 2.f;

	return bl + ex * w * u1 + ey * h * u2;
}

f32 Rectangle::SampleDir( vec3f & rayDir, const vec3f & pos, const f32 u1, const f32 u2 ) const
{
	const vec3f pk = SamplePoint( u1, u2 );
	rayDir = pk - pos;
	const f32 rayLenSq = Dot( rayDir, rayDir );
	const f32 rayLen = std::sqrtf( rayLenSq );
	rayDir /= rayLen;

	const f32 costheta = -Dot( ez, rayDir );

	return costheta / rayLenSq;
}

f32 Rectangle::SolidAngle( const vec3f & integrationPos ) const
{
	const vec3f q0 = p0 - integrationPos;
	const vec3f q1 = p1 - integrationPos;
	const vec3f q2 = p2 - integrationPos;
	const vec3f q3 = p3 - integrationPos;

	vec3f n0 = Normalize( Cross( q0, q1 ) );
	vec3f n1 = Normalize( Cross( q1, q2 ) );
	vec3f n2 = Normalize( Cross( q2, q3 ) );
	vec3f n3 = Normalize( Cross( q3, q0 ) );

	const f32 alpha = std::acosf( -Dot( n0, n1 ) );
	const f32 beta = std::acosf( -Dot( n1, n2 ) );
	const f32 gamma = std::acosf( -Dot( n2, n3 ) );
	const f32 zeta = std::acosf( -Dot( n3, n0 ) );

	return alpha + beta + gamma + zeta - 2 * M_PI;
}

f32 Rectangle::IntegrateStructuredSampling( const vec3f & integrationPos, const vec3f & integrationNrm ) const
{
	// Solving E(n) = Int_lightArea [ Lin <n.l> dl ] == lightArea * Lin * Average[<n.l>]
	// With Average[<n.l>] approximated with the average of the 4 corners and center of the rect

	// unit space solid angle (== unit space area)
	f32 solidAngle = SolidAngle( integrationPos );

	// unit space vectors to the 5 sample points

	vec3f q0 = Normalize( p0 - integrationPos );
	vec3f q1 = Normalize( p1 - integrationPos );
	vec3f q2 = Normalize( p2 - integrationPos );
	vec3f q3 = Normalize( p3 - integrationPos );
	vec3f q4 = Normalize( position - integrationPos );

	// area * Average[<n.l>] (Lin is 1.0)
	return solidAngle * 0.2f * (
		std::max( 0.f, Dot( q0, integrationNrm ) ) +
		std::max( 0.f, Dot( q1, integrationNrm ) ) +
		std::max( 0.f, Dot( q2, integrationNrm ) ) +
		std::max( 0.f, Dot( q3, integrationNrm ) ) +
		std::max( 0.f, Dot( q4, integrationNrm ) )
		);
}

f32 Rectangle::IntegrateMRP( const vec3f & integrationPos, const vec3f & integrationNrm ) const
{
	const vec3f d0p = -ez;
	const vec3f d1p = integrationNrm;

	const f32 nDotpN = std::max( 0.f, Dot( integrationNrm, ez ) );

	vec3f d0 = Normalize( d0p + integrationNrm * nDotpN );
	vec3f d1 = Normalize( d1p - ez * nDotpN );

	vec3f dh = Normalize( d0 + d1 );

	Plane rectPlane = { position, ez };
	vec3f pH = rectPlane.RayIntersection( integrationPos, dh );
	pH = rectPlane.ClampPointInRect( *this, pH );

	const f32 solidAngle = SolidAngle( integrationPos );

	vec3f rayDir = Normalize( pH - integrationPos );

	return solidAngle * std::max( 0.f, Dot( integrationNrm, rayDir ) );
}

f32 Rectangle::IntegrateAngularStratification( const vec3f & integrationPos, const vec3f & integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand ) const
{
	const u32 sampleCountX = (u32) std::sqrt( (f32) sampleCount );
	const u32 sampleCountY = sampleCountX;

	// bottom left point
	const vec3f a0 = position - ex * hx - ey * hy;

	const vec3f W1 = position - ex * hx;
	const vec3f W2 = position + ex * hx;
	const vec3f H1 = position - ey * hy;
	const vec3f H2 = position + ey * hy;

	const f32 lw1_2 = Dot( W1, W1 );
	const f32 lw2_2 = Dot( W2, W2 );
	const f32 lh1_2 = Dot( H1, H1 );
	const f32 lh2_2 = Dot( H2, H2 );

	const f32 rwidth = 2.f * hx;
	const f32 rwidth_2 = rwidth * rwidth;
	const f32 rheight = 2.f * hy;
	const f32 rheight_2 = rheight * rheight;

	const f32 lw1 = std::sqrtf( lw1_2 );
	const f32 lw2 = std::sqrtf( lw2_2 );
	const f32 lh1 = std::sqrtf( lh1_2 );
	const f32 lh2 = std::sqrtf( lh2_2 );

	const f32 cosx = -Dot( W1, ex ) / lw1;
	const f32 sinx = std::sqrtf( 1.f - cosx * cosx );
	const f32 cosy = -Dot( H1, ey ) / lh1;
	const f32 siny = std::sqrtf( 1.f - cosy * cosy );

	const f32 dx = 1.f / sampleCountX;
	const f32 dy = 1.f / sampleCountY;

	const f32 theta = std::acosf( ( lw1_2 + lw2_2 - rwidth_2 ) * 0.5f / ( lw1 * lw2 ) );
	const f32 gamma = std::acosf( ( lh1_2 + lh2_2 - rheight_2 ) * 0.5f / ( lh1 * lh2 ) );
	const f32 theta_n = theta * dx;
	const f32 gamma_n = gamma * dy;

	const f32 tanW = std::tanf( theta_n );
	const f32 tanH = std::tanf( gamma_n );

	const int nCoeff = nBand * nBand;
	std::vector<f32> shtmp( nCoeff );

	// Marching over the equi angular rectangles
	f32 x1 = 0.f;
	f32 tanx1 = 0.f;
	for ( u32 x = 0; x < sampleCountX; ++x )
	{
		const f32 tanx2 = ( tanx1 + tanW ) / ( 1.f - tanx1 * tanW );
		const f32 x2 = lw1 * tanx2 / ( sinx + tanx2 * cosx );
		const f32 lx = x2 - x1;

		f32 y1 = 0.f;
		f32 tany1 = 0.f;
		for ( u32 y = 0; y < sampleCountY; ++y )
		{
			const f32 tany2 = ( tany1 + tanH ) / ( 1.f - tany1 * tanH );
			const f32 y2 = lh1 * tany2 / ( siny + tany2 * cosy );
			const f32 ly = y2 - y1;

			const f32 u1 = ( x1 + Random::GlobalPool.Next() * lx ) / rwidth;
			const f32 u2 = ( y1 + Random::GlobalPool.Next() * ly ) / rheight;
			//const f32 u1 = (x1 + Random::Float() * lx) / rwidth;
			//const f32 u2 = (y1 + Random::Float() * ly) / rheight;

			vec3f rayDir;
			const f32 invPdf = SampleDir( rayDir, integrationPos, u1, u2 ) * lx * ly * sampleCount;

			if ( invPdf > 0.f )
			{
				SHEval( nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0] );

				for ( int i = 0; i < nCoeff; ++i )
				{
					shvals[i] += shtmp[i] * invPdf; // constant luminance of 1 for now
				}
			}

			y1 = y2;
			tany1 = tany2;
		}

		x1 = x2;
		tanx1 = tanx2;
	}

	return 1.f / (f32) sampleCount;
}

f32 Rectangle::IntegrateRandom( const vec3f & integrationPos, const vec3f & integrationNrm, u32 sampleCount, std::vector<f32>& shvals, int nBand ) const
{
	// Rectangle area
	const f32 area = 4.f * hx * hy;

	const int nCoeff = nBand * nBand;

	std::vector<f32> shtmp( nCoeff, 0.f );

	// costheta * A / r^3
	for ( u32 i = 0; i < sampleCount; ++i )
	{
		const vec2f randV( Random::GlobalPool.Next(), Random::GlobalPool.Next() );
		//const vec2f randV = Random::Vec2f();

		vec3f rayDir;
		const f32 invPdf = SampleDir( rayDir, integrationPos, randV.x, randV.y );

		if ( invPdf > 0.f )
		{
			SHEval( nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0] );

			for ( int j = 0; j < nCoeff; ++j )
			{
				shvals[j] += shtmp[j] * invPdf; // constant luminance of 1 for now
			}
		}
	}

	return area / (f32) sampleCount;
}


void SphericalRectangle::Init( const Rectangle &rect, const vec3f &org )
{
	o = org;
	const f32 w = rect.hx * 2.f;
	const f32 h = rect.hy * 2.f;

	// Compute Local reference system R (4.1)
	x = rect.ex;
	y = rect.ey;
	z = rect.ez;

	const vec3f s = rect.p0; // left-bottom vertex or rectangle
	const vec3f d = s - org;

	x0 = Dot( d, x );
	y0 = Dot( d, y );
	z0 = Dot( d, z );

	// flip Z if necessary, it should be away from Q
	if ( z0 > 0.f )
	{
		z0 *= -1.f;
		z *= -1.f;
	}

	z0sq = z0 * z0;
	x1 = x0 + w;
	y1 = y0 + h;
	y0sq = y0 * y0;
	y1sq = y1 * y1;

	// Compute Solid angle subtended by Q (4.2)
	const vec3f v00( x0, y0, z0 );
	const vec3f v01( x0, y1, z0 );
	const vec3f v10( x1, y0, z0 );
	const vec3f v11( x1, y1, z0 );

	vec3f n0 = Normalize( Cross( v00, v10 ) );
	vec3f n1 = Normalize( Cross( v10, v11 ) );
	vec3f n2 = Normalize( Cross( v11, v01 ) );
	vec3f n3 = Normalize( Cross( v01, v00 ) );

	const f32 g0 = std::acosf( -Dot( n0, n1 ) );
	const f32 g1 = std::acosf( -Dot( n1, n2 ) );
	const f32 g2 = std::acosf( -Dot( n2, n3 ) );
	const f32 g3 = std::acosf( -Dot( n3, n0 ) );

	S = g0 + g1 + g2 + g3 - 2.f * M_PI;

	// Additional constants for future sampling
	b0 = n0.z;
	b1 = n2.z;
	b0sq = b0 * b0;
	k = 2.f * M_PI - g2 - g3;
}

vec3f SphericalRectangle::Sample( f32 u1, f32 u2 ) const
{
	// compute cu
	const f32 phi_u = u1 * S + k;
	const f32 fu = ( std::cosf( phi_u ) * b0 - b1 ) / std::sinf( phi_u );

	f32 cu = sign( fu ) / std::sqrtf( fu * fu + b0sq );
	cu = std::max( -1.f, std::min( 1.f, cu ) );

	// compute xu
	f32 xu = -( cu * z0 ) / std::sqrtf( 1.f - cu * cu );
	xu = std::max( x0, std::min( x1, xu ) ); // bound the result in spherical width

												// compute yv
	const f32 d = std::sqrtf( xu*xu + z0sq );
	const f32 h0 = y0 / std::sqrtf( d*d + y0sq );
	const f32 h1 = y1 / std::sqrtf( d*d + y1sq );

	const f32 hv = h0 + u2 * ( h1 - h0 ); const f32 hvsq = hv * hv;
	const f32 yv = ( hvsq < ( 1.f - 1e-6f ) ) ? hv * d / std::sqrtf( 1.f - hvsq ) : y1;

	// transform to world coordinates
	return o + x * xu + y * yv + z * z0;
}

f32 SphericalRectangle::Integrate( const vec3f & integrationNrm, u32 sampleCount, std::vector<f32>& shvals, int nBand ) const
{
	// Construct a Spherical Rectangle from the point integrationPos

	const f32 area = S; // spherical rectangle area/solidangle


	const int nCoeff = nBand * nBand;
	std::vector<f32> shtmp( nCoeff );

	// Sample the spherical rectangle
	for ( u32 i = 0; i < sampleCount; ++i )
	{
		const vec2f randV( Random::GlobalPool.Next(), Random::GlobalPool.Next() );
		//const vec2f randV = Random::Vec2f();

		vec3f rayDir = Sample( randV.x, randV.y ) - o;
		const f32 rayLenSq = Dot( rayDir, rayDir );
		rayDir = Normalize( rayDir );

		SHEval( nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0] );

		for ( int j = 0; j < nCoeff; ++j )
		{
			shvals[j] += shtmp[j];
		}
	}

	return area / (f32) sampleCount;
}

void PlanarRectangle::InitBary( const Rectangle & rect, const vec3f & integrationPoint )
{
	// copy src points here
	std::memcpy( &p0, &rect.p0, 4 * sizeof( vec3f ) );

	const vec3f bary = ( p0 + p1 + p2 + p3 ) * 0.25f;

	vec3f org = Normalize( bary - integrationPoint );

	const f32 rayLenSqr = Dot( org, org );
	const f32 rayLen = std::sqrtf( rayLenSqr );
	const vec3f nrm = org / rayLen;

	// org is bary in relative coordinates
	const Plane P{ org, nrm };

	// project each point on the plane P
	vec3f *verts = &p0;
	for ( int i = 0; i < 4; ++i )
	{
		vec3f rayDir = Normalize( verts[i] - integrationPoint );

		// with 0 as origin since the vertices are in relative coordinates already
		verts[i] = P.RayIntersection( vec3f( 0 ), rayDir );
	}

	const vec3f d0 = p1 - p0;
	const vec3f d1 = p3 - p0;

	const vec3f d2 = p2 - p3;
	const vec3f d3 = p2 - p1;

	w = Len( d0 );
	h = Len( d1 );

	ex = d0 / w;
	ey = d1 / h;
	ez = nrm; // normal pointing away from origin

	area = 0.5f * ( w * h + Len( d2 ) * Len( d3 ) );
}

void PlanarRectangle::InitUnit( const Rectangle & rect, const vec3f & integrationPoint )
{
	// copy src points here
	std::memcpy( &p0, &rect.p0, 4 * sizeof( vec3f ) );

	// Normalize each vertex to get the inscribed triangle,
	// and find the average vertex plane
	vec3f bary( 0 );
	vec3f *verts = &p0;
	for ( int i = 0; i < 4; ++i )
	{
		verts[i] = Normalize( verts[i] - integrationPoint ); // put in intPos frame
		bary += verts[i];
	}

	bary *= 0.25f;

	// project each point on the tangent plane at the barycenter
	f32 rayLenSq = Dot( bary, bary );
	f32 rayLen = std::sqrtf( rayLenSq );
	ez = bary / rayLen; // plane normal

	// bary is in relative coordinates to the integration pt already
	const Plane P{ bary, ez };

	for ( int i = 0; i < 4; ++i )
	{
		// with 0 as origin since the vertices are in relative coordinates already
		verts[i] = P.RayIntersection( vec3f( 0 ), verts[i] );
	}

	const vec3f d0 = p1 - p0;
	const vec3f d1 = p3 - p0;

	const vec3f d2 = p2 - p3;
	const vec3f d3 = p2 - p1;

	w = Len( d0 );
	h = Len( d1 );

	ex = d0 / w;
	ey = d1 / h;

	area = 0.5f * ( w * h + Len( d2 ) * Len( d3 ) );
}

vec3f PlanarRectangle::SamplePoint( f32 u1, f32 u2 ) const
{
	const f32 mu1 = 1.f - u1;
	const f32 mu2 = 1.f - u2;
	return p0 * mu1 * mu2 + p1 * u1 * mu2 + p2 * u1 * u2 + p3 * mu1 * u2;
}

f32 PlanarRectangle::SampleDir( vec3f & rayDir, const f32 u1, const f32 u2 ) const
{
	const vec3f pk = SamplePoint( u1, u2 );
	rayDir = pk;
	const f32 rayLenSq = Dot( rayDir, rayDir );
	const f32 rayLen = std::sqrtf( rayLenSq );
	rayDir /= rayLen;

	const f32 costheta = Dot( ez, rayDir );

	return costheta / rayLenSq;
}

f32 PlanarRectangle::IntegrateRandom( u32 sampleCount, std::vector<f32> &shvals, int nBand ) const
{
	const int nCoeff = nBand * nBand;

	std::vector<f32> shtmp( nCoeff, 0.f );

	// costheta * A / r^2
	for ( u32 i = 0; i < sampleCount; ++i )
	{
		const vec2f randV( Random::GlobalPool.Next(), Random::GlobalPool.Next() );
		//const vec2f randV = Random::Vec2f();

		vec3f rayDir;
		const f32 invPdf = SampleDir( rayDir, randV.x, randV.y );

		if ( invPdf > 0.f )
		{
			SHEval( nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0] );

			for ( int j = 0; j < nCoeff; ++j )
			{
				shvals[j] += shtmp[j] * invPdf; // constant luminance of 1 for now
			}
		}
	}

	return area / (f32) sampleCount;
}