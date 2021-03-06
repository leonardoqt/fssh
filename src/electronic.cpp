#include "electronic.h"
#include <iostream>

using namespace arma;

// TODO: may change from init N0_s to rho0_fock
void electronic::construct_rho_fock()
{
	switch(sz_s)
	{
	case(1):
	{
		rho_fock(1,1) = N_s(0,0);
		rho_fock(0,0) = cx_double(1,0) - rho_fock(1,1);
		break;
	}
	case(2):
	{
		rho_fock(3,3) = N_s(0,0) * N_s(1,1);
		rho_fock(1,1) = N_s(0,0) - rho_fock(3,3);
		rho_fock(2,2) = N_s(1,1) - rho_fock(3,3);
		rho_fock(0,0) = cx_double(1,0) - N_s(0,0) - N_s(1,1) + rho_fock(3,3);
		rho_fock(1,2) = N_s(0,1);
		rho_fock(2,1) = N_s(1,0);
		break;
	}
	}
}

void electronic::init_rho(mat N0_s, potential& HH, double Beta, double Damping, double Dt)
{
	not_evolve = 1;
	sz_s = HH.sz_s;
	sz_t = HH.sz_t;
	sz_f = HH.sz_f;
	beta = Beta;
	damping = Damping;
	dt = Dt;
	N_b = join_vert(zeros<vec>(sz_s),1/(1+exp(beta * HH.Eb)));
	N_t = zeros<cx_mat>(sz_t,sz_t);
	N_s = zeros<cx_mat>(sz_s,sz_s);
	rho_fock = zeros<cx_mat>(sz_f,sz_f);
	N_t.diag() = cx_vec( N_b, zeros<vec>(sz_t) );
	N_t(span(0,sz_s-1),span(0,sz_s-1)) = cx_mat (N0_s,zeros<mat>(sz_s,sz_s));
	//
	N_s = N_t(span(0,sz_s-1),span(0,sz_s-1));
	//
	construct_rho_fock();
	rho_fock_old = rho_fock;
	//
	hop_bath = zeros<mat>(sz_f,sz_f);
}

void electronic::evolve(potential& HH, double x_last2, double x_last, double x_now)
{
	// Use RK4
	// Ndot = -i[H rho] - damping*(rho_b - rho_eq)
	x_2 = x_last2;
	x_1 = x_last;
	x_0 = x_now;
	if (not_evolve)
	{
		not_evolve = 0;
		H_t = HH.Ht(x_0);
		H_t_t1 = HH.Ht(x_1);
		H_t_t2 = HH.Ht(x_2);
	}
	else
	{
		H_t_t2 = H_t;
		H_t = HH.Ht(x_0);
		H_t_t1 = HH.Ht(x_1);
	}
	//
	cx_mat k1,k2,k3,k4;
	k1 = N_dot(H_t_t2,N_t);
	k2 = N_dot(H_t_t1,N_t + dt/2*k1);
	k3 = N_dot(H_t_t1,N_t + dt/2*k2);
	k4 = N_dot(H_t   ,N_t + dt  *k3);
	N_t = N_t + (k1+2*k2+2*k3+k4)*(dt/6);
	//
	N_s = N_t(span(0,sz_s-1),span(0,sz_s-1));
	vec Es, Ef, F_tmp;
	mat Us, U_tmp;
	HH.adiab_H_s(x_0,Es,Us);
	N_s = Us.t() * N_s * Us;
	rho_fock_old = rho_fock;
	construct_rho_fock();
	//
	drho = ( rho_fock - rho_fock_old );
	HH.adiab_H_f(x_1,Ef,F_tmp,U_tmp);
	mat hh = diagmat(Ef);
	cx_mat dd = HH.ddt_f(x_2,x_0);
	drho_2fit = drho + cx_double(0,1)*dt*( hh*rho_fock_old - rho_fock_old*hh ) + ( dd*rho_fock_old - rho_fock_old*dd );
	drho_2fit /= dt;
}

cx_mat electronic::N_dot(mat H0, cx_mat N0)
{
	cx_mat res = cx_double(0,-1)*(H0*N0 - N0*H0);
	N0(span(0,sz_s-1),span(0,sz_s-1)) = zeros<cx_mat>(sz_s,sz_s);
	N0.diag() = N0.diag() - cx_vec( N_b, zeros<vec>(sz_t) );
	res = res - damping*N0;
	return res;
}

//void electronic::fit_drho_v1(potential& HH)
//{
//	//
//	//   0,  1,  2,  3,  4,  5,  6,  7
//	// L01,L10,L23,L32,L02,L20,L13,L31;
//	// L01 -> |0><1|, which hops from 1 to 0
//	cube LL(4,4,8,fill::zeros);
//	mat L;
//	LL(0,1,0) = LL(1,0,1) = LL(2,3,2) = LL(3,2,3) = 1;
//	LL(0,2,4) = LL(2,0,5) = LL(1,3,6) = LL(3,1,7) = 1;
//	//
//	cx_mat rho_dot0(4,4,fill::zeros);
//	cx_mat rho_dot1(4,4,fill::zeros);
//	cx_mat rho_dot2(4,4,fill::zeros);
//	cx_mat rho_dot3(4,4,fill::zeros);
//	// lambda_01/lambda_10 = exp(beta*E), i.e.,
//	// lambda_10 = lambda_01 * exp(-beta*E)
//	L = LL.slice(0);
//	rho_dot0 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(1);
//	rho_dot0 += exp(-beta*HH.eigval_s(0,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	L = LL.slice(2);
//	rho_dot1 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(3);
//	rho_dot1 += exp(-beta*HH.eigval_s(0,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	//
//	L = LL.slice(4);
//	rho_dot2 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(5);
//	rho_dot2 += exp(-beta*HH.eigval_s(1,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	L = LL.slice(6);
//	rho_dot3 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(7);
//	rho_dot3 += exp(-beta*HH.eigval_s(1,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	//
//	// fitting: write rho as a column vector, V = drho_2fit, M = [rho_dot1, rho_dot2]
//	// then Lambda = (M\dagger M)^(-1)*Re(M\dagger V), where Lambda = [l1; l2]
//	vec V = real(drho_2fit.diag());
//	mat M = real(join_horiz(rho_dot0.diag(),rho_dot1.diag(),rho_dot2.diag(),rho_dot3.diag()));
//	V = V.rows(0,2);
//	M = M.rows(0,2);
//	mat MM = M.t()*M;
//	vec MV = M.t()*V;
//	vec Lambda = solve(MM+eye(4,4)*norm(MM)*1e-6,MV);
//	//cout<<norm(M*Lambda-V)/norm(V)<<endl;
//	//V.t().print("target");
//	//(M*Lambda-V).t().print("res");
//	//
//	// put results into hop_bath, where hop_bath(i,j) is from i to j, i.e., |j><i|
//	hop_bath = zeros<mat>(4,4);
//	hop_bath(0,1) = Lambda(0)*exp(-beta*HH.eigval_s(0,AA.ind_pre)); hop_bath(1,0) = Lambda(0);
//	hop_bath(2,3) = Lambda(1)*exp(-beta*HH.eigval_s(0,AA.ind_pre)); hop_bath(3,2) = Lambda(1);
//	hop_bath(0,2) = Lambda(2)*exp(-beta*HH.eigval_s(1,AA.ind_pre)); hop_bath(2,0) = Lambda(2);
//	hop_bath(1,3) = Lambda(3)*exp(-beta*HH.eigval_s(1,AA.ind_pre)); hop_bath(3,1) = Lambda(3);
//	//Lambda.t().print();
//}

//void electronic::fit_drho_v2(potential& HH)
//{
//	//
//	//   0,  1,  2,  3,  4,  5,  6,  7
//	// L01,L10,L23,L32,L02,L20,L13,L31;
//	// L01 -> |0><1|, which hops from 1 to 0
//	cube LL(4,4,8,fill::zeros);
//	mat L;
//	LL(0,1,0) = LL(1,0,1) = LL(2,3,2) = LL(3,2,3) = 1;
//	LL(0,2,4) = LL(2,0,5) = LL(1,3,6) = LL(3,1,7) = 1;
//	//
//	cx_mat rho_dot0(4,4,fill::zeros);
//	cx_mat rho_dot1(4,4,fill::zeros);
//	cx_mat rho_dot2(4,4,fill::zeros);
//	cx_mat rho_dot3(4,4,fill::zeros);
//	// lambda_01/lambda_10 = exp(beta*E), i.e.,
//	// lambda_10 = lambda_01 * exp(-beta*E)
//	L = LL.slice(0);
//	rho_dot0 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(1);
//	rho_dot0 += exp(-beta*HH.eigval_s(0,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	L = LL.slice(2);
//	rho_dot1 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(3);
//	rho_dot1 += exp(-beta*HH.eigval_s(0,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	//
//	L = LL.slice(4);
//	rho_dot2 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(5);
//	rho_dot2 += exp(-beta*HH.eigval_s(1,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	L = LL.slice(6);
//	rho_dot3 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
//	L = LL.slice(7);
//	rho_dot3 += exp(-beta*HH.eigval_s(1,AA.ind_pre)) * ( L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2 );
//	//
//	// fitting: write rho as a column vector, V = drho_2fit, M = [rho_dot1, rho_dot2]
//	// then Lambda = (M\dagger M)^(-1)*Re(M\dagger V), where Lambda = [l1; l2]
//	cx_vec V = drho_2fit.as_col();
//	cx_mat M = join_horiz(rho_dot0.as_col(),rho_dot1.as_col(),rho_dot2.as_col(),rho_dot3.as_col());
//	mat MM = real(M.t()*M);
//	vec MV = real(M.t()*V);
//	vec Lambda = pinv(MM+1e-8*eye(4,4))*MV;
//	//reshape(V,4,4).print("target");
//	//reshape(M*Lambda-V,4,4).print("res");
//	//exit(EXIT_FAILURE);
//	//
//	// put results into hop_bath, where hop_bath(i,j) is from i to j, i.e., |j><i|
//	hop_bath = zeros<mat>(4,4);
//	hop_bath(0,1) = Lambda(0)*exp(-beta*HH.eigval_s(0,AA.ind_pre)); hop_bath(1,0) = Lambda(0);
//	hop_bath(2,3) = Lambda(1)*exp(-beta*HH.eigval_s(0,AA.ind_pre)); hop_bath(3,2) = Lambda(1);
//	hop_bath(0,2) = Lambda(2)*exp(-beta*HH.eigval_s(1,AA.ind_pre)); hop_bath(2,0) = Lambda(2);
//	hop_bath(1,3) = Lambda(3)*exp(-beta*HH.eigval_s(1,AA.ind_pre)); hop_bath(3,1) = Lambda(3);
//	Lambda.t().print();
//	//vec eigval;
//	//mat tmp;
//	//eig_sym(eigval,tmp,MM);
//	//cout<<eigval(0)<<'\t'<<Lambda(1)<<'\t'<<Lambda(2)<<'\t'<<Lambda(3)<<'\t'<<eigval(0)<<'\t'<<eigval(1)<<'\t'<<eigval(2)<<'\t'<<eigval(3)<<endl;
//	//real(drho_2fit.diag()).t().print();
//	//real(rho_fock.diag()).t().print();
//}

void electronic::fit_drho_v3()
{
	// the jump operator corresponding to the same single-electron
	// hops can be merged if only looking at the diagonal of rho
	//
	//   0,  1,  2,  3,  4,  5,  6,  7
	// L01,L10,L23,L32,L02,L20,L13,L31;
	// L01 -> |0><1|, which hops from 1 to 0
	cube LL(4,4,8,fill::zeros);
	mat L;
	LL(0,1,0) = LL(1,0,1) = LL(2,3,2) = LL(3,2,3) = 1;
	LL(0,2,4) = LL(2,0,5) = LL(1,3,6) = LL(3,1,7) = 1;
	//
	cx_mat rho_dot0(4,4,fill::zeros);
	cx_mat rho_dot1(4,4,fill::zeros);
	cx_mat rho_dot2(4,4,fill::zeros);
	cx_mat rho_dot3(4,4,fill::zeros);
	// lambda_01/lambda_10 = exp(beta*E), i.e.,
	// lambda_10 = lambda_01 * exp(-beta*E)
	L = LL.slice(0);
	rho_dot0 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(2);
	rho_dot0 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(1);
	rho_dot1 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(3);
	rho_dot1 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	//
	L = LL.slice(4);
	rho_dot2 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(6);
	rho_dot2 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(5);
	rho_dot3 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(7);
	rho_dot3 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	//
	// fitting: write rho as a column vector, V = drho_2fit, M = [rho_dot1, rho_dot2]
	// then Lambda = (M\dagger M)^(-1)*Re(M\dagger V), where Lambda = [l1; l2]
	vec V = real(drho_2fit.diag());
	mat M = real(join_horiz(rho_dot0.diag(),rho_dot1.diag(),rho_dot2.diag(),rho_dot3.diag()));
	V = V.rows(0,2);
	M = M.rows(0,2);
	mat MM = M.t()*M;
	vec MV = M.t()*V;
	vec Lambda = solve(MM+eye(4,4)*norm(MM)*1e-12,MV);
	//cout<<norm(M*Lambda-V)/norm(V)<<endl;
	//V.t().print("target");
	//(M*Lambda-V).t().print("res");
	//exit(EXIT_FAILURE);
	//
	// put results into hop_bath, where hop_bath(i,j) is from i to j, i.e., |j><i|
	hop_bath = zeros<mat>(4,4);
	hop_bath(1,0) = hop_bath(3,2) = Lambda(0);
	hop_bath(0,1) = hop_bath(2,3) = Lambda(1);
	hop_bath(2,0) = hop_bath(3,1) = Lambda(2);
	hop_bath(0,2) = hop_bath(1,3) = Lambda(3);
	//Lambda.t().print();
}

void electronic::fit_drho_v3_1imp()
{
	// the jump operator corresponding to the same single-electron
	// hops can be merged if only looking at the diagonal of rho
	//
	//   0,  1
	// L01,L10
	// L01 -> |0><1|, which hops from 1 to 0
	cube LL(sz_f,sz_f,2,fill::zeros);
	mat L;
	LL(0,1,0) = LL(1,0,1) = 1;
	//
	cx_mat rho_dot0(sz_f,sz_f,fill::zeros);
	cx_mat rho_dot1(sz_f,sz_f,fill::zeros);
	// lambda_01/lambda_10 = exp(beta*E), i.e.,
	// lambda_10 = lambda_01 * exp(-beta*E)
	L = LL.slice(0);
	rho_dot0 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	L = LL.slice(1);
	rho_dot1 += L*rho_fock_old*L.t() - (L.t()*L*rho_fock_old + rho_fock_old*L.t()*L)/2;
	//
	// fitting: write rho as a column vector, V = drho_2fit, M = [rho_dot1, rho_dot2]
	// then Lambda = (M\dagger M)^(-1)*Re(M\dagger V), where Lambda = [l1; l2]
	vec V = real( drho_2fit.diag() );
	mat M = real( join_horiz(rho_dot0.diag(),rho_dot1.diag()) );
	V = V.rows(0,sz_f-2);
	M = M.rows(0,sz_f-2);
	mat MM = M.t()*M;
	vec MV = M.t()*V;
	vec Lambda = solve(MM+eye(sz_f,sz_f)*norm(MM)*1e-8,MV);
	//cout<<norm(M*Lambda-V)/norm(V)<<endl;
	//V.t().print("target");
	//(M*Lambda-V).t().print("res");
	//exit(EXIT_FAILURE);
	//
	// put results into hop_bath, where hop_bath(i,j) is from i to j, i.e., |j><i|
	hop_bath = zeros<mat>(sz_f,sz_f);
	hop_bath(1,0) = Lambda(0);
	hop_bath(0,1) = Lambda(1);
	//Lambda.t().print();
}

void electronic::fit_drho(potential &HH, int method)
{
	switch(sz_s)
	{
	case(1):
	{
		switch(method)
		{
		case(3):
		{
			fit_drho_v3_1imp();
			break;
		}
		}
		break;
	}
	case(2):
	{
		switch(method)
		{
		case(1):
		{
			//fit_drho_v1(HH);
			break;
		}
		case(2):
		{
			//fit_drho_v2(HH);
			break;
		}
		case(3):
		{
			fit_drho_v3();
			break;
		}
		}
		break;
	}
	}
}

//void electronic::try_decoherence(ionic& AA)
//{
//	if (AA.v_pre*AA.v_new <0 && AA.istate >0)
//	{
//		psi = psi * 0;
//		psi(AA.istate) = cx_double(1,0);
//		rho = psi * psi.t();
//	}
//}
