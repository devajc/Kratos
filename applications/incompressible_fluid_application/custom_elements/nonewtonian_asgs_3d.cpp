/*
==============================================================================
KratosIncompressibleFluidApplication 
A library based on:
Kratos
A General Purpose Software for Multi-Physics Finite Element Analysis
Version 1.0 (Released on march 05, 2007).

Copyright 2007
Pooyan Dadvand, Riccardo Rossi
pooyan@cimne.upc.edu 
rrossi@cimne.upc.edu
- CIMNE (International Center for Numerical Methods in Engineering),
Gran Capita' s/n, 08034 Barcelona, Spain


Permission is hereby granted, free  of charge, to any person obtaining
a  copy  of this  software  and  associated  documentation files  (the
"Software"), to  deal in  the Software without  restriction, including
without limitation  the rights to  use, copy, modify,  merge, publish,
distribute,  sublicense and/or  sell copies  of the  Software,  and to
permit persons to whom the Software  is furnished to do so, subject to
the following condition:

Distribution of this code for  any  commercial purpose  is permissible
ONLY BY DIRECT ARRANGEMENT WITH THE COPYRIGHT OWNERS.

The  above  copyright  notice  and  this permission  notice  shall  be
included in all copies or substantial portions of the Software.

THE  SOFTWARE IS  PROVIDED  "AS  IS", WITHOUT  WARRANTY  OF ANY  KIND,
EXPRESS OR  IMPLIED, INCLUDING  BUT NOT LIMITED  TO THE  WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT  SHALL THE AUTHORS OR COPYRIGHT HOLDERS  BE LIABLE FOR ANY
CLAIM, DAMAGES OR  OTHER LIABILITY, WHETHER IN AN  ACTION OF CONTRACT,
TORT  OR OTHERWISE, ARISING  FROM, OUT  OF OR  IN CONNECTION  WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

==============================================================================
 */

//   
//   Project Name:        Kratos       
//   Last modified by:    $Author: kazem $
//   Date:                $Date: 2009-01-21 14:15:02 $
//   Revision:            $Revision: 1.6 $
//
//

//#define GRADPN_FORM
//#define STOKES

// System includes 


// External includes 


// Project includes 
#include "includes/define.h"
#include "custom_elements/nonewtonian_asgs_3d.h"
#include "utilities/math_utils.h"
#include "incompressible_fluid_application.h"
#include "utilities/geometry_utilities.h" 

namespace Kratos {

    //************************************************************************************
    //************************************************************************************

    NoNewtonianASGS3D::NoNewtonianASGS3D(IndexType NewId, GeometryType::Pointer pGeometry)
    : Element(NewId, pGeometry) {
        //DO NOT ADD DOFS HERE!!!
    }

    //************************************************************************************
    //************************************************************************************

    NoNewtonianASGS3D::NoNewtonianASGS3D(IndexType NewId, GeometryType::Pointer pGeometry, PropertiesType::Pointer pProperties)
    : Element(NewId, pGeometry, pProperties) {

    }

    Element::Pointer NoNewtonianASGS3D::Create(IndexType NewId, NodesArrayType const& ThisNodes, PropertiesType::Pointer pProperties) const {

        KRATOS_TRY
        return Element::Pointer(new NoNewtonianASGS3D(NewId, GetGeometry().Create(ThisNodes), pProperties));
        KRATOS_CATCH("");
    }

    NoNewtonianASGS3D::~NoNewtonianASGS3D() {
    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateLocalSystem(MatrixType& rLeftHandSideMatrix, VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) {
        KRATOS_TRY
                // KRATOS_WATCH("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ NoNewtonianASGS element ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
                int nodes_number = 4;
        int dim = 3;
        unsigned int matsize = nodes_number * (dim + 1);

        if (rLeftHandSideMatrix.size1() != matsize)
            rLeftHandSideMatrix.resize(matsize, matsize); //false says not to preserve existing storage!!

        if (rRightHandSideVector.size() != matsize)
            rRightHandSideVector.resize(matsize); //false says not to preserve existing storage!!


        noalias(rLeftHandSideMatrix) = ZeroMatrix(matsize, matsize);
        noalias(rRightHandSideVector) = ZeroVector(matsize);

        double delta_t = rCurrentProcessInfo[DELTA_TIME];

	boost::numeric::ublas::bounded_matrix<double, 4, 3 > DN_DX;
        array_1d<double, 4 > N; 
	
        //getting data for the given geometry
        double Volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);

        double tauone;
        double tautwo;
        CalculateTau(DN_DX,N,tauone, tautwo, delta_t, Volume, rCurrentProcessInfo);

        //add body force and momentum
        AddBodyForceAndMomentum(rRightHandSideVector,DN_DX, N, delta_t, Volume, tauone, tautwo);


        //       //add projections
        //       if (rCurrentProcessInfo[OSS_SWITCH] == 1.0)
        //           AddProjectionForces(rRightHandSideVector, DN_DX, Volume, tauone, tautwo);


        KRATOS_CATCH("")
    }
    //***********************************************************************************++
    //**************************************************************************************++

    void NoNewtonianASGS3D::CalculateRightHandSide(VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) {
        KRATOS_TRY

        MatrixType temp = Matrix();
        CalculateLocalSystem(temp, rRightHandSideVector, rCurrentProcessInfo);

        KRATOS_CATCH("")
    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateMassContribution(MatrixType& K, const double time, const double volume) {
        KRATOS_TRY
                double lump_mass_fac = volume * 0.25;
        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        int nodes_number = 4;
        int dof = 3;
        for (int nd = 0; nd < nodes_number; nd++) {
            int row = nd * (dof + 1);
            for (int jj = 0; jj < dof; jj++)
                K(row + jj, row + jj) += density * lump_mass_fac;
        }

        KRATOS_CATCH("")

    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::MassMatrix(MatrixType& rMassMatrix, ProcessInfo& rCurrentProcessInfo) {
        KRATOS_TRY

                //lumped
                unsigned int dimension = GetGeometry().WorkingSpaceDimension();
        unsigned int NumberOfNodes = GetGeometry().size();
        unsigned int MatSize = (dimension + 1) * NumberOfNodes;
        if (rMassMatrix.size1() != MatSize)
            rMassMatrix.resize(MatSize, MatSize, false);

        rMassMatrix = ZeroMatrix(MatSize, MatSize);
        double delta_t = rCurrentProcessInfo[DELTA_TIME];

	boost::numeric::ublas::bounded_matrix<double, 4, 3 > DN_DX;
        array_1d<double, 4 > N; 
	
        //getting data for the given geometry
        double Volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);

        //Calculate tau
        double tauone;
        double tautwo;
        CalculateTau(DN_DX,N,tauone, tautwo, delta_t, Volume, rCurrentProcessInfo);

        CalculateMassContribution(rMassMatrix, delta_t, Volume);
        /**Stablization*/
        //add stablilization terms due to advective term (a)grad(V) * ro*Acce
        CalculateAdvMassStblTerms(rMassMatrix, DN_DX, N, tauone, Volume);
        //add stablilization terms due to grad term grad(q) * ro*Acce
        CalculateGradMassStblTerms(rMassMatrix, DN_DX,N, tauone, Volume);

        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateLocalVelocityContribution(MatrixType& rDampMatrix, VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) {
        KRATOS_TRY
                int nodes_number = 4;
        int dim = 3;
        unsigned int matsize = nodes_number * (dim + 1);

        if (rDampMatrix.size1() != matsize)
            rDampMatrix.resize(matsize, matsize, false); //false says not to preserve existing storage!!


        noalias(rDampMatrix) = ZeroMatrix(matsize, matsize);

        double delta_t = rCurrentProcessInfo[DELTA_TIME];

	boost::numeric::ublas::bounded_matrix<double, 4, 3 > DN_DX;
        array_1d<double, 4 > N; 
	
        //getting data for the given geometry
        double Volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);

        /**         LHS           */
        /**Advective term*/
        double tauone;
        double tautwo;
        CalculateTau(DN_DX,N,tauone, tautwo, delta_t, Volume, rCurrentProcessInfo);

        CalculateAdvectiveTerm(rDampMatrix, DN_DX,N, tauone, tautwo, delta_t, Volume);
        /**Calculate Pressure term + divergence term of pressure equation*/
        CalculatePressureTerm(rDampMatrix, DN_DX, N, delta_t, Volume);

        //compute projections
        /**Stablization*/
        //stabilization terms
        CalculateDivStblTerm(rDampMatrix, DN_DX, tautwo, Volume);
        CalculateAdvStblAllTerms(rDampMatrix, rRightHandSideVector, DN_DX, N, tauone, delta_t, Volume);
        CalculateGradStblAllTerms(rDampMatrix, rRightHandSideVector, DN_DX,N, delta_t, tauone, Volume);
        //KRATOS_WATCH(rRightHandSideVector);
        /**         RHS           */
        /**Internal Forces*/
        CalculateResidual(DN_DX,rDampMatrix, rRightHandSideVector, Volume);
        /**         LHS           */
        /**Viscous term*/
        CalculateViscousTerm(rDampMatrix, DN_DX, Volume);
// KRATOS_WATCH("LINE 267")
        KRATOS_CATCH("")
    }


    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateViscousTerm(MatrixType& K, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const double volume) {
        KRATOS_TRY
        double mu;
        int nodes_number = 4;
        int dof = 3;
        double density;
        calculatedensity(GetGeometry(), density, mu);
        boost::numeric::ublas::bounded_matrix<double, 6, 12 > B = ZeroMatrix(6, 12);
        boost::numeric::ublas::bounded_matrix<double, 6, 6 > C = ZeroMatrix(6, 6);
        boost::numeric::ublas::bounded_matrix<double, 12, 12 > temp = ZeroMatrix(12, 12);
	
        double app_mu;
	array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
		
        //calculating operator B
        CalculateB(B, DN_DX);
//         KRATOS_WATCH(B)
	
	//Bingham Fluid
       CalculateApparentViscosity(app_mu, grad_sym_vel, B, mu);
	//Newtonian Fluid: leave decommented the CalculateApparentViscosity (we need grad_sym_vel) and decomment the following line
	// Remember to modify CalculateResidualand CalculateTau.
// 	app_mu = mu;
// 	KRATOS_WATCH(app_mu)
        C(0, 0) = 2.0;
        C(0, 1) = 0.0;
        C(0, 2) = 0.0;
	C(0, 3) = 0.0;
	C(0, 4) = 0.0;
	C(0, 5) = 0.0;
	
        C(1, 0) = 0.0;
        C(1, 1) = 2.0;
        C(1, 2) = 0.0;
	C(1, 3) = 0.0;
	C(1, 4) = 0.0;
	C(1, 5) = 0.0;
        
	C(2, 0) = 0.0;
        C(2, 1) = 0.0;
        C(2, 2) = 2.0;
	C(2, 3) = 0.0;
	C(2, 4) = 0.0;
	C(2, 5) = 0.0;
        
	C(3, 0) = 0.0;
        C(3, 1) = 0.0;
        C(3, 2) = 0.0;
	C(3, 3) = 1.0;
	C(3, 4) = 0.0;
	C(3, 5) = 0.0;
	
	C(4, 0) = 0.0;
        C(4, 1) = 0.0;
        C(4, 2) = 0.0;
	C(4, 3) = 0.0;
	C(4, 4) = 1.0;
	C(4, 5) = 0.0;
	
	C(5, 0) = 0.0;
        C(5, 1) = 0.0;
        C(5, 2) = 0.0;
	C(5, 3) = 0.0;
	C(5, 4) = 0.0;
	C(5, 5) = 1.0;
        
        C *= app_mu;

//         KRATOS_WATCH(C)
        //Calculating the viscous contribution to the LHS int(Btrans C B)dA
        //         temp = prod(trans(B),prod(C,B));
        for (unsigned int i = 0; i < B.size2(); i++) {
            for (unsigned int j = 0; j < B.size2(); j++) {
                for (unsigned int l = 0; l < C.size1(); l++) {
                    for (unsigned int k = 0; k < C.size1(); k++) {
                        temp(i, j) += B(l, i) * C(l, k) * B(k, j);
                    }
                }
            }
        }
//         KRATOS_WATCH(temp)
        temp *= volume;
        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1);
                int loc_column = jj*dof;

                K(row, column) += temp(loc_row, loc_column);
                K(row, column + 1) += temp(loc_row, loc_column + 1);
		K(row, column + 2) += temp(loc_row, loc_column + 2);
		
		K(row + 1, column) += temp(loc_row + 1, loc_column);
                K(row + 1, column + 1) += temp(loc_row + 1, loc_column + 1);
		K(row + 1, column + 2) += temp(loc_row + 1, loc_column + 2);

                K(row + 2, column) += temp(loc_row + 2, loc_column);
                K(row + 2, column + 1) += temp(loc_row + 2, loc_column + 1);
                K(row + 2, column + 2) += temp(loc_row + 2, loc_column + 2);
                
            }
        }

        KRATOS_CATCH("")
    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateAdvectiveTerm(MatrixType& K, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double, 4 > & N, const double tauone, const double tautwo, const double time, const double volume) {
        KRATOS_TRY
                
                //calculate mean advective velocity and taus
                const array_1d<double, 3 > & adv_vel0 = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel0 = GetGeometry()[0].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel1 = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel1 = GetGeometry()[1].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel2 = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel2 = GetGeometry()[2].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel3 = GetGeometry()[3].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel3 = GetGeometry()[3].FastGetSolutionStepValue(MESH_VELOCITY);
        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

	array_1d<double,3> ms_adv_vel;
        ms_adv_vel[0] = N[0]*(adv_vel0[0] - mesh_vel0[0]) + N[1]*(adv_vel1[0] - mesh_vel1[0]) + N[2]*(adv_vel2[0] - mesh_vel2[0]) + N[3]*(adv_vel3[0] - mesh_vel3[0]);
        ms_adv_vel[1] = N[0]*(adv_vel0[1] - mesh_vel0[1]) + N[1]*(adv_vel1[1] - mesh_vel1[1]) + N[2]*(adv_vel2[1] - mesh_vel2[1]) + N[3]*(adv_vel3[1] - mesh_vel3[1]);
        ms_adv_vel[2] = N[0]*(adv_vel0[2] - mesh_vel0[2]) + N[1]*(adv_vel1[2] - mesh_vel1[2]) + N[2]*(adv_vel2[2] - mesh_vel2[2]) + N[3]*(adv_vel3[2] - mesh_vel3[2]);

        //calculate convective term
        int nodes_number = 4;
        int dof = 3;
        int matsize = dof*nodes_number;

        boost::numeric::ublas::bounded_matrix<double, 3, 12 > conv_opr = ZeroMatrix(dof, matsize);
        boost::numeric::ublas::bounded_matrix<double, 12, 3 > shape_func = ZeroMatrix(matsize, dof);

        for (int ii = 0; ii < nodes_number; ii++) {
            int column = ii*dof;
            conv_opr(0, column) = DN_DX(ii, 0) * ms_adv_vel[0] + DN_DX(ii, 1) * ms_adv_vel[1] + DN_DX(ii, 2) * ms_adv_vel[2];
            conv_opr(1, column + 1) = conv_opr(0, column);
            conv_opr(2, column + 2) = conv_opr(0, column);

            shape_func(column, 0) = N[ii];
            shape_func(column + 1, 1) = shape_func(column, 0);
            shape_func(column + 2, 2) = shape_func(column, 0);
        }
        boost::numeric::ublas::bounded_matrix<double, 12, 12 > temp_convterm = ZeroMatrix(matsize, matsize);
        temp_convterm = prod(shape_func, conv_opr);
	temp_convterm *= volume * density;
        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1);
                int loc_column = jj*dof;

                K(row, column) += temp_convterm(loc_row, loc_column);
                K(row + 1, column + 1) += temp_convterm(loc_row + 1, loc_column + 1);
                K(row + 2, column + 2) += temp_convterm(loc_row + 2, loc_column + 2);
            }
        }

        KRATOS_CATCH("")

    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculatePressureTerm(MatrixType& K, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double, 4 > & N, const double time, const double volume) {
        KRATOS_TRY
                int nodes_number = 4;
        int dof = 3;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);


        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1) + dof;
                //**************************************************************
                //Elemental gradient of pressure term (momentum equation)
                K(row, column) -= volume * N(jj) * DN_DX(ii, 0);
                K(row + 1, column) -= volume * N(jj) * DN_DX(ii, 1);
		K(row + 2, column) -=  volume * N(jj) * DN_DX(ii, 2);
                //**************************************************************
                // 	       //Elemental divergence terms (continuity equation)
                // 	       // Fomulation n1  int( q * rho * Div( u ))
                K(column, row) += volume * density * N(jj) * DN_DX(ii, 0);
                K(column, row + 1) += volume * density * N(jj) * DN_DX(ii, 1);
		K(column, row + 2) +=  volume * density * N(jj) * DN_DX(ii, 2);

            }
        }


        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateDivStblTerm(MatrixType& K, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const double tautwo, const double volume) {
        KRATOS_TRY
                int nodes_number = 4;
        int dof = 3;
        int matsize = dof*nodes_number;

        boost::numeric::ublas::bounded_matrix<double, 1, 12 > div_opr = ZeroMatrix(1, matsize);
        for (int ii = 0; ii < nodes_number; ii++) {
            int index = dof*ii;
            div_opr(0, index) = DN_DX(ii, 0);
            div_opr(0, index + 1) = DN_DX(ii, 1);
            div_opr(0, index + 2) = DN_DX(ii, 2);
        }


        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);


        boost::numeric::ublas::bounded_matrix<double, 12, 12 > temp_div = ZeroMatrix(matsize, matsize);
        temp_div = tautwo * prod(trans(div_opr), div_opr);
	temp_div *= volume * density;
        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1);
                int loc_column = jj*dof;

                K(row, column) +=  temp_div(loc_row, loc_column);
                K(row, column + 1) +=  temp_div(loc_row, loc_column + 1);
                K(row, column + 2) +=   temp_div(loc_row, loc_column + 2);

                K(row + 1, column) +=  temp_div(loc_row + 1, loc_column);
                K(row + 1, column + 1) += temp_div(loc_row + 1, loc_column + 1);
                K(row + 1, column + 2) +=  temp_div(loc_row + 1, loc_column + 2);

                K(row + 2, column) +=  temp_div(loc_row + 2, loc_column);
                K(row + 2, column + 1) +=  temp_div(loc_row + 2, loc_column + 1);
                K(row + 2, column + 2) +=  temp_div(loc_row + 2, loc_column + 2);

            }
        }

        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateAdvStblAllTerms(MatrixType& K, VectorType& F, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double, 4 > & N, const double tauone, const double time, const double volume) {
        KRATOS_TRY
                const array_1d<double, 3 > & adv_vel0 = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel0 = GetGeometry()[0].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel1 = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel1 = GetGeometry()[1].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel2 = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel2 = GetGeometry()[2].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel3 = GetGeometry()[3].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel3 = GetGeometry()[3].FastGetSolutionStepValue(MESH_VELOCITY);

	array_1d<double,3> ms_adv_vel;
        ms_adv_vel[0] = N[0]*(adv_vel0[0] - mesh_vel0[0]) + N[1]*(adv_vel1[0] - mesh_vel1[0]) + N[2]*(adv_vel2[0] - mesh_vel2[0]) + N[3]*(adv_vel3[0] - mesh_vel3[0]);
        ms_adv_vel[1] = N[0]*(adv_vel0[1] - mesh_vel0[1]) + N[1]*(adv_vel1[1] - mesh_vel1[1]) + N[2]*(adv_vel2[1] - mesh_vel2[1]) + N[3]*(adv_vel3[1] - mesh_vel3[1]);
        ms_adv_vel[2] = N[0]*(adv_vel0[2] - mesh_vel0[2]) + N[1]*(adv_vel1[2] - mesh_vel1[2]) + N[2]*(adv_vel2[2] - mesh_vel2[2]) + N[3]*(adv_vel3[2] - mesh_vel3[2]);

        //ms_adv_vel[0] = 0.0;
        //ms_adv_vel[1] = 0.0;

        //calculate convective term
        int nodes_number = 4;
        int dof = 3;
        int matsize = dof*nodes_number;

        boost::numeric::ublas::bounded_matrix<double, 3, 12 > conv_opr = ZeroMatrix(dof, matsize);
        boost::numeric::ublas::bounded_matrix<double, 12, 3 > shape_func = ZeroMatrix(matsize, dof);

        for (int ii = 0; ii < nodes_number; ii++) {
            int column = ii*dof;
            conv_opr(0, column) = DN_DX(ii, 0) * ms_adv_vel[0] + DN_DX(ii, 1) * ms_adv_vel[1] + DN_DX(ii, 2) * ms_adv_vel[2];
            conv_opr(1, column + 1) = conv_opr(0, column);
            conv_opr(2, column + 2) = conv_opr(0, column);

            shape_func(column, 0) = N[ii];
            shape_func(column + 1, 1) = shape_func(column, 0);
            shape_func(column + 2, 2) = shape_func(column, 0);
        }

        //build (a.grad V)(ro*a.grad U) stabilization term & assemble
        boost::numeric::ublas::bounded_matrix<double, 12, 12 > adv_stblterm = ZeroMatrix(matsize, matsize);
        adv_stblterm = tauone * prod(trans(conv_opr), conv_opr);


        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

	adv_stblterm *= volume * density ;
        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1);
                int loc_column = jj*dof;

                K(row, column) += adv_stblterm(loc_row, loc_column);
                K(row, column + 1) += adv_stblterm(loc_row, loc_column + 1);
                K(row, column + 2) += adv_stblterm(loc_row, loc_column + 2);

                K(row + 1, column) +=  adv_stblterm(loc_row + 1, loc_column);
                K(row + 1, column + 1) += adv_stblterm(loc_row + 1, loc_column + 1);
                K(row + 1, column + 2) += adv_stblterm(loc_row + 1, loc_column + 2);

                K(row + 2, column) += adv_stblterm(loc_row + 2, loc_column);
                K(row + 2, column + 1) += adv_stblterm(loc_row + 2, loc_column + 1);
                K(row + 2, column + 2) += adv_stblterm(loc_row + 2, loc_column + 2);
           }
        }

        //build 1*tau1*(a.grad V)(grad P) & 1*tau1*(grad q)(ro*a.grad U) stabilization terms & assemble
        boost::numeric::ublas::bounded_matrix<double, 12, 4 > grad_stblterm = ZeroMatrix(matsize, nodes_number);
        grad_stblterm = tauone * prod(trans(conv_opr), trans(DN_DX));

        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1) + dof;

                K(row, column) += volume * grad_stblterm(loc_row, jj);
                K(row + 1, column) += volume * grad_stblterm(loc_row + 1, jj);
                K(row + 2, column) += volume  * grad_stblterm(loc_row + 2, jj);

                K(column, row) += volume * density * grad_stblterm(loc_row, jj);
                K(column, row + 1) += volume * density * grad_stblterm(loc_row + 1, jj);
                K(column, row + 2) += volume * density * grad_stblterm(loc_row + 2, jj);
            }
        }

        /*
        //tau1*ro/dt*U(n+1,i+1).(1.0*a.grad V)
        boost::numeric::ublas::bounded_matrix<double,6,6> temp_convterm = ZeroMatrix(matsize,matsize);
        temp_convterm = prod(trans(conv_opr),trans(shape_func));

        double fac = tauone/time*density;
	
        for ( int ii = 0; ii < nodes_number; ii++)
            {
                int row = ii*(dof+1);
                int loc_row = ii*dof;
                for( int jj=0; jj < nodes_number; jj++)
                   {
                        int column = jj*(dof+1);
                        int loc_column = jj*dof;

                        K(row,column) += area*fac*temp_convterm(loc_row,loc_column);
                        K(row + 1,column + 1) += area*fac*temp_convterm(loc_row + 1,loc_column + 1);
                   }
            }

	
        //build (1.0*a.grad V) (Fbody + ro/dt * U(n)) stabilization term & assemble
        array_1d<double,2> bdf = ZeroVector(2);
        const array_1d<double,2> bdf0 = GetGeometry()[0].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double,2> bdf1 = GetGeometry()[1].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double,2> bdf2 = GetGeometry()[2].FastGetSolutionStepValue(BODY_FORCE);

        const array_1d<double,3>& acce0 = GetGeometry()[0].FastGetSolutionStepValue(ACCELERATION);
        const array_1d<double,3>& acce1 = GetGeometry()[1].FastGetSolutionStepValue(ACCELERATION);
        const array_1d<double,3>& acce2 = GetGeometry()[2].FastGetSolutionStepValue(ACCELERATION);

<<<<<<< .mine
	for(int ii = 0; ii< nodes_number; ++ii)
	  {
		int index = ii*(dof + 1);DN_DX
		int loc_index = ii*dof;
		F[index] += 1.0*area*fbd_stblterm[loc_index];
		F[index + 1] += 1.0*area*fbd_stblterm[loc_index + 1];
	  }
=======
        bdf[0] = N[0]*(density*bdf0[0] + density/time * acce0[0] ) +  N[1]*(density*bdf1[0] + density/time * acce1[0]) + N[2]*(density*bdf2[0] + density/time * acce2[0]);
        bdf[1] =  N[0]*(density*bdf0[1] + density/time * acce0[1] ) +  N[1]*(density*bdf1[1] + density/time * acce1[1]) + N[2]*(density*bdf2[1] + density/time * acce2[1]);
>>>>>>> .r469
	

        array_1d<double,6> fbd_stblterm = ZeroVector(matsize);
        fbd_stblterm = tauone *1.0* prod(trans(conv_opr),bdf);

        for(int ii = 0; ii< nodes_number; ++ii)
          {
                int index = ii*(dof + 1);
                int loc_index = ii*dof;
                F[index] += 1.0*area*fbd_stblterm[loc_index];
                F[index + 1] += 1.0*area*fbd_stblterm[loc_index + 1];
          }
	
         */
        //build (1.0*a.grad V) (Fbody) stabilization term & assemble
        array_1d<double, 3 > bdf = ZeroVector(3);
        const array_1d<double, 3 > bdf0 = GetGeometry()[0].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf1 = GetGeometry()[1].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf2 = GetGeometry()[2].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf3 = GetGeometry()[3].FastGetSolutionStepValue(BODY_FORCE);


        bdf[0] = N[0]*(density * bdf0[0]) + N[1]*(density * bdf1[0]) + N[2]*(density * bdf2[0]) + N[3]*(density * bdf3[0]);
        bdf[1] = N[0]*(density * bdf0[1]) + N[1]*(density * bdf1[1]) + N[2]*(density * bdf2[1]) + N[3]*(density * bdf3[1]);
        bdf[2] = N[0]*(density * bdf0[2]) + N[1]*(density * bdf1[2]) + N[2]*(density * bdf2[2]) + N[3]*(density * bdf3[2]);


        array_1d<double, 12 > fbd_stblterm = ZeroVector(matsize);
        fbd_stblterm = tauone * 1.0 * prod(trans(conv_opr), bdf);

        for (int ii = 0; ii < nodes_number; ++ii) {
            int index = ii * (dof + 1);
            int loc_index = ii*dof;
            F[index] += 1.0 * volume * fbd_stblterm[loc_index];
            F[index + 1] += 1.0 * volume * fbd_stblterm[loc_index + 1];
            F[index + 2] += 1.0 * volume * fbd_stblterm[loc_index + 2];
        }
        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateAdvMassStblTerms(MatrixType& M, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double, 4 > & N, const double tauone, const double volume) {
        KRATOS_TRY

        const array_1d<double, 3 > & adv_vel0 = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel0 = GetGeometry()[0].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel1 = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel1 = GetGeometry()[1].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel2 = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel2 = GetGeometry()[2].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel3 = GetGeometry()[3].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel3 = GetGeometry()[3].FastGetSolutionStepValue(MESH_VELOCITY);

	array_1d<double, 3 > ms_adv_vel;
        ms_adv_vel[0] = N[0]*(adv_vel0[0] - mesh_vel0[0]) + N[1]*(adv_vel1[0] - mesh_vel1[0]) + N[2]*(adv_vel2[0] - mesh_vel2[0]) + N[3]*(adv_vel3[0] - mesh_vel3[0]);
        ms_adv_vel[1] = N[0]*(adv_vel0[1] - mesh_vel0[1]) + N[1]*(adv_vel1[1] - mesh_vel1[1]) + N[2]*(adv_vel2[1] - mesh_vel2[1]) + N[3]*(adv_vel3[1] - mesh_vel3[1]);
        ms_adv_vel[2] = N[0]*(adv_vel0[2] - mesh_vel0[2]) + N[1]*(adv_vel1[2] - mesh_vel1[2]) + N[2]*(adv_vel2[2] - mesh_vel2[2]) + N[3]*(adv_vel3[2] - mesh_vel3[2]);

        //ms_adv_vel[0] = 0.0;
        //ms_adv_vel[1] = 0.0;

        //calculate convective term
        int nodes_number = 4;
        int dof = 3;
        int matsize = dof*nodes_number;

        //calculate density
        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        boost::numeric::ublas::bounded_matrix<double, 3, 12 > conv_opr = ZeroMatrix(dof, matsize);
        boost::numeric::ublas::bounded_matrix<double, 12, 3 > shape_func = ZeroMatrix(matsize, dof);

        for (int ii = 0; ii < nodes_number; ii++) {
            int column = ii*dof;
            conv_opr(0, column) = DN_DX(ii, 0) * ms_adv_vel[0] + DN_DX(ii, 1) * ms_adv_vel[1] + DN_DX(ii, 2) * ms_adv_vel[2];
            conv_opr(1, column + 1) = conv_opr(0, column);
            conv_opr(2, column + 2) = conv_opr(0, column);

            shape_func(column, 0) = N[ii];
            shape_func(column + 1, 1) = shape_func(column, 0);
            shape_func(column + 2, 2) = shape_func(column, 0);
        }

        //tau1*ro*Nacc.(1.0*a.grad V)
        boost::numeric::ublas::bounded_matrix<double, 12, 12 > temp_convterm = ZeroMatrix(matsize, matsize);
        temp_convterm = prod(shape_func, conv_opr);

        double fac = tauone*density * volume;

        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            int loc_row = ii*dof;
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1);
                int loc_column = jj*dof;

                M(row, column) += fac * temp_convterm(loc_row, loc_column);
                M(row + 1, column + 1) += fac * temp_convterm(loc_row + 1, loc_column + 1);
                M(row + 2, column + 2) += fac * temp_convterm(loc_row + 2, loc_column + 2);
            }
        }


        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateGradStblAllTerms(MatrixType& K, VectorType& F, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double,4>& N, const double time, const double tauone, const double volume) {
        KRATOS_TRY
                int nodes_number = 4;
        int dof = 3;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        //build 1*(grad q . grad p) stabilization term & assemble
        boost::numeric::ublas::bounded_matrix<double, 4, 4 > gard_opr = ZeroMatrix(nodes_number, nodes_number);
        gard_opr = 1.0 * tauone * prod(DN_DX, trans(DN_DX));

        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1) + dof;

            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1) + dof;

                K(row, column) += volume * 1.0 * gard_opr(ii, jj);

            }
        }
        /*
        //build 1*tau1*ro/deltat U grad q)
        double fac = tauone*density/time;
        for ( int ii = 0; ii < nodes_number; ii++)
            {
                int row = ii*(dof+1);
                for( int jj=0; jj < nodes_number; jj++)
                   {
                        int column = jj*(dof+1) + dof;

                        //K(row,column) += -1*area * fac* N(ii) * DN_DX(jj,0);
                        K(column,row) += 1*area * fac* N(ii) * DN_DX(jj,0);

                        //K(row + 1,column) += -1*area * fac* N(ii) * DN_DX(jj,1);
                        K(column,row + 1) += 1*area * fac* N(ii) * DN_DX(jj,1);
                   }
            }

	
        //build 1*(grad q) (Fbody + ro/dt * U(n+1,i)) stabilization term & assemble
        array_1d<double,2> bdf = ZeroVector(2);
        const array_1d<double,2> bdf0 = GetGeometry()[0].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double,2> bdf1 = GetGeometry()[1].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double,2> bdf2 = GetGeometry()[2].FastGetSolutionStepValue(BODY_FORCE);


        const array_1d<double,3>& vel0_n = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY,1);
        const array_1d<double,3>& vel1_n = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY,1);
        const array_1d<double,3>& vel2_n = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY,1);


        bdf[0] = N[0]*(density*bdf0[0] + density/time * vel0_n[0] ) +  N[1]*(density*bdf1[0] + density/time * vel1_n[0]) + N[2]*(density*bdf2[0] + density/time * vel2_n[0]);
        bdf[1] =  N[0]*(density*bdf0[1] + density/time * vel0_n[1] ) +  N[1]*(density*bdf1[1] + density/time * vel1_n[1]) + N[2]*(density*bdf2[1] + density/time * vel2_n[1]);

        array_1d<double,3> fbd_stblterm = ZeroVector(nodes_number);
        fbd_stblterm = tauone * prod(DN_DX,bdf);
	

        for(int ii = 0; ii< nodes_number; ++ii)
          {
                int index = ii*(dof + 1) + dof;
                F[index] += 1*area*fbd_stblterm[ii];
          }*/


        //build 1*(grad q) (Fbody ) stabilization term & assemble
        array_1d<double, 3 > bdf = ZeroVector(3);
        const array_1d<double, 3 > bdf0 = GetGeometry()[0].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf1 = GetGeometry()[1].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf2 = GetGeometry()[2].FastGetSolutionStepValue(BODY_FORCE);
        const array_1d<double, 3 > bdf3 = GetGeometry()[3].FastGetSolutionStepValue(BODY_FORCE);


        bdf[0] = N[0]*(density * bdf0[0]) + N[1]*(density * bdf1[0]) + N[2]*(density * bdf2[0]) + N[3]*(density * bdf3[0]);
        bdf[1] = N[0]*(density * bdf0[1]) + N[1]*(density * bdf1[1]) + N[2]*(density * bdf2[1]) + N[3]*(density * bdf3[1]);
        bdf[2] = N[0]*(density * bdf0[2]) + N[1]*(density * bdf1[2]) + N[2]*(density * bdf2[2]) + N[3]*(density * bdf3[2]);

        array_1d<double, 4 > fbd_stblterm = ZeroVector(nodes_number);
        fbd_stblterm = tauone * prod(DN_DX, bdf);


        for (int ii = 0; ii < nodes_number; ++ii) {
            int index = ii * (dof + 1) + dof;
            F[index] += 1.0 * volume * fbd_stblterm[ii];
        }

        KRATOS_CATCH("")

    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateGradMassStblTerms(MatrixType& M, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const array_1d<double,4>& N, const double tauone, const double volume) {
        KRATOS_TRY
                int nodes_number = 4;
        int dof = 3;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        //build 1*tau1*ro Nacc grad q)
        double fac = tauone*density;
        for (int ii = 0; ii < nodes_number; ii++) {
            int row = ii * (dof + 1);
            for (int jj = 0; jj < nodes_number; jj++) {
                int column = jj * (dof + 1) + dof;

                //K(row,column) += -1*area * fac* N(ii) * DN_DX(jj,0);
                M(column, row) += volume * fac * N[ii] * DN_DX(jj, 0);

                //K(row + 1,column) += -1*area * fac* N(ii) * DN_DX(jj,1);
                M(column, row + 1) += volume * fac * N[ii] * DN_DX(jj, 1);

                M(column, row + 2) += volume * fac * N[ii] * DN_DX(jj, 2);
            }
        }

        KRATOS_CATCH("")

    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::AddBodyForceAndMomentum(VectorType& F,const boost::numeric::ublas::bounded_matrix<double,4,3>& DN_DX, const array_1d<double, 4 > & N, const double time, const double volume, const double tauone, const double tautwo) {
        KRATOS_TRY
                int nodes_number = 4;
        int dof = 3;


        //double lump_mass_fac = area * 0.333333333333333333333333;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        //for Arhenious
        int matsize = dof*nodes_number;
        boost::numeric::ublas::bounded_matrix<double, 1, 12 > div_opr = ZeroMatrix(1, matsize);
        for (int ii = 0; ii < nodes_number; ii++) {
            int index = dof*ii;
            div_opr(0, index) = DN_DX(ii, 0);
            div_opr(0, index + 1) = DN_DX(ii, 1);
            div_opr(0, index + 2) = DN_DX(ii, 2);
        }
        const double ar_0 = GetGeometry()[0].FastGetSolutionStepValue(ARRHENIUS);
        const double ar_1 = GetGeometry()[1].FastGetSolutionStepValue(ARRHENIUS);
        const double ar_2 = GetGeometry()[2].FastGetSolutionStepValue(ARRHENIUS);
        const double ar_3 = GetGeometry()[3].FastGetSolutionStepValue(ARRHENIUS);

        double mean_ar = 0.25 * (ar_0 + ar_1 + ar_2 + ar_3);


        //body  & momentum term force
        for (int ii = 0; ii < nodes_number; ii++) {
            int index = ii * (dof + 1);
            int loc_index = ii * dof ;
            const array_1d<double, 3 > bdf = GetGeometry()[ii].FastGetSolutionStepValue(BODY_FORCE);


            F[index] += volume * N[ii] * density * bdf[0];
            F[index + 1] += volume * N[ii] * density * bdf[1];
            F[index + 2] += volume * N[ii] * density * bdf[2];


            //arrhenius
            F[index + 3] += (volume * N[ii] * mean_ar);
            F[index] += tautwo * volume * mean_ar * div_opr(0, loc_index);
            F[index + 1] += tautwo * volume * mean_ar * div_opr(0, loc_index + 1);
            F[index + 2] += tautwo * volume * mean_ar * div_opr(0, loc_index + 2);
        }


        KRATOS_CATCH("")
    }
    //************************************************************************************
    //************************************************************************************
	void NoNewtonianASGS3D::CalculateResidual(const boost::numeric::ublas::bounded_matrix<double,4,3>& DN_DX, const MatrixType& K, VectorType& F, const double volume) {
		KRATOS_TRY

		int nodes_number = 4;
		int dof = 3;

		array_1d<double, 16 > UP = ZeroVector(16);
		for (int ii = 0; ii < nodes_number; ++ii) {
		    int index = ii * (dof + 1);
		    UP[index] = GetGeometry()[ii].FastGetSolutionStepValue(VELOCITY)[0];
		    UP[index + 1] = GetGeometry()[ii].FastGetSolutionStepValue(VELOCITY)[1];
		    UP[index + 2] = GetGeometry()[ii].FastGetSolutionStepValue(VELOCITY)[2];
		    UP[index + 3] = GetGeometry()[ii].FastGetSolutionStepValue(PRESSURE);
		}

		/*Rest Viscous Forces from dampmatrix*/
		double mu;
		double density;
		calculatedensity(GetGeometry(), density, mu);

		boost::numeric::ublas::bounded_matrix<double, 6, 12 > B = ZeroMatrix(6, 12);
		boost::numeric::ublas::bounded_matrix<double, 6, 6 > C = ZeroMatrix(6, 6);
		array_1d<double, 12 > auxDevStressVector = ZeroVector(12);
		array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
		double app_mu;

		array_1d<double, 6 > aux_1 = ZeroVector(6);

		//calculating operator B
		CalculateB(B, DN_DX);
		// KRATOS_WATCH(B)
		/**Calculating residual vector */
		F -= prod(K, UP);

		/**Add Bt*sigma_dev (Viscous Forces)*/
		//sigma dev intern

	// 	Bingham Fluid:
		CalculateApparentViscosity(app_mu, grad_sym_vel, B, mu);
	// 	Newtonian Fluid: Leave Decommented the CalculateApparentviscosity (we need grad_sym_vel) and decomment the following line
	//	Remember to modify CalculateViscousTerm and CalculateTau
	// 	app_mu = mu;
		
		aux_1 = 2 * app_mu * grad_sym_vel;
		aux_1[3] *= 0.5; //considering Voigt notation for the gradient of velocity (alternative to the C matrix of the viscous term.
		aux_1[4] *= 0.5;
		aux_1[5] *= 0.5;
		auxDevStressVector = prod(trans(B), aux_1);
		/*TO DECOMMENT*/

		for (int ii = 0; ii < nodes_number; ii++) {
		    int row = ii * (dof + 1);
		    int loc_row = ii * dof;

		    F[row] -= auxDevStressVector[loc_row] * volume;
		    F[row + 1] -= auxDevStressVector[loc_row + 1] * volume;

		}


        KRATOS_CATCH("")
    }


    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::ComputeProjections(array_1d<double, 12 > & adv_proj, array_1d<double, 4 > & div_proj, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const double tauone, const double tautwo, const array_1d<double, 4 > & N, const double volume, const double time) {
        unsigned int number_of_nodes = GetGeometry().PointsNumber();
        unsigned int dim = 3;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        const array_1d<double, 3 > & adv_vel0 = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel0 = GetGeometry()[0].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel1 = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel1 = GetGeometry()[1].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel2 = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel2 = GetGeometry()[2].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel3 = GetGeometry()[3].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel3 = GetGeometry()[3].FastGetSolutionStepValue(MESH_VELOCITY);

	array_1d<double, 3 > ms_adv_vel;
        ms_adv_vel[0] = N[0]*(adv_vel0[0] - mesh_vel0[0]) + N[1]*(adv_vel1[0] - mesh_vel1[0]) + N[2]*(adv_vel2[0] - mesh_vel2[0]) + N[3]*(adv_vel3[0] - mesh_vel3[0]);
        ms_adv_vel[1] = N[0]*(adv_vel0[1] - mesh_vel0[1]) + N[1]*(adv_vel1[1] - mesh_vel1[1]) + N[2]*(adv_vel2[1] - mesh_vel2[1]) + N[3]*(adv_vel3[1] - mesh_vel3[1]);
        ms_adv_vel[2] = N[0]*(adv_vel0[2] - mesh_vel0[2]) + N[1]*(adv_vel1[2] - mesh_vel1[2]) + N[2]*(adv_vel2[2] - mesh_vel2[2]) + N[3]*(adv_vel3[2] - mesh_vel3[2]);


        double const_adv_proj_X = 0.0;
        double const_adv_proj_Y = 0.0;
       double const_adv_proj_Z = 0.0;
        double mean_div_proj = 0.0;
        array_1d<double, 3 > mean_new_vel = ZeroVector(3);
        array_1d<double, 3 > mean_old_vel = ZeroVector(3);
        array_1d<double, 3 > mean_bdf = ZeroVector(3);

        for (unsigned int i = 0; i < number_of_nodes; i++) {

            //int index = i*dim;
            double pr = GetGeometry()[i].FastGetSolutionStepValue(PRESSURE);
            const array_1d<double, 3 > & vel = GetGeometry()[i].FastGetSolutionStepValue(VELOCITY);
            const array_1d<double, 3 > & old_vel = GetGeometry()[i].FastGetSolutionStepValue(VELOCITY, 1);
            const array_1d<double, 3 > & bdf = GetGeometry()[i].FastGetSolutionStepValue(BODY_FORCE);

            //const array_1d<double,2>& bdf = GetGeometry()[i].FastGetSolutionStepValue(BODY_FORCE);
            // to consider the jump gradp/ro is calculated
            //pr = pr/density;

            //adv_proj = PI(ro*dv/dt + ro*a.gradU + gradP - f) considering lumped mass PI() = ()
            //calculate constant part of RES ->ro*a.gradU + gradP
            const_adv_proj_X += (pr * DN_DX(i, 0) + density * (ms_adv_vel[0] * DN_DX(i, 0) + ms_adv_vel[1] * DN_DX(i, 1)+ ms_adv_vel[2] * DN_DX(i, 2)) * vel[0]);
            const_adv_proj_Y += (pr * DN_DX(i, 1) + density * (ms_adv_vel[0] * DN_DX(i, 0) + ms_adv_vel[1] * DN_DX(i, 1)+ ms_adv_vel[2] * DN_DX(i, 2)) * vel[1]);
            const_adv_proj_Z += (pr * DN_DX(i, 2) + density * (ms_adv_vel[0] * DN_DX(i, 0) + ms_adv_vel[1] * DN_DX(i, 1)+ ms_adv_vel[2] * DN_DX(i, 2)) * vel[2]);

            //div_proj = PI(ro*divU)
            mean_div_proj += density * (DN_DX(i, 0) * vel[0] + DN_DX(i, 1) * vel[1] + DN_DX(i, 2) * vel[2]);

            //calcuale mean velocity and body force
            mean_new_vel += 0.25 * vel;
            mean_old_vel += 0.25 * old_vel;
            mean_bdf += 0.25 * bdf;
        }


        for (unsigned int i = 0; i < number_of_nodes; i++) {
            int index = i*dim;

            adv_proj[index] = volume * N[i]*(density * (mean_new_vel[0] - mean_old_vel[0]) / time + const_adv_proj_X - density * mean_bdf[0]);
            adv_proj[index + 1] = volume * N[i]*(density * (mean_new_vel[1] - mean_old_vel[1]) / time + const_adv_proj_Y - density * mean_bdf[1]);
            adv_proj[index + 2] = volume * N[i]*(density * (mean_new_vel[2] - mean_old_vel[2]) / time + const_adv_proj_Z - density * mean_bdf[2]);

            div_proj[i] = volume * N[i] * density*mean_div_proj;

            //update projections
            array_1d<double, 3 > & advtermproj = GetGeometry()[i].FastGetSolutionStepValue(ADVPROJ);
            advtermproj[0] += adv_proj[index];
            advtermproj[1] += adv_proj[index + 1];
            advtermproj[2] += adv_proj[index + 2];

            double& divtermproj = GetGeometry()[i].FastGetSolutionStepValue(DIVPROJ);
            divtermproj += div_proj[i];



            //calculate nodal area

            GetGeometry()[i].FastGetSolutionStepValue(NODAL_AREA) += 0.25 * volume;

        }

        /*for (unsigned int i=0;i<number_of_nodes;i++)
         {
                int index = i*dim;
                const double pr = GetGeometry()[i].FastGetSolutionStepValue(PRESSURE);
                const array_1d<double,3>& vel = GetGeometry()[i].FastGetSolutionStepValue(VELOCITY);

                const array_1d<double,3>& mesh_vel = GetGeometry()[i].FastGetSolutionStepValue(MESH_VELOCITY);
                array_1d<double,2> adv_vel = ZeroVector(2);
                adv_vel[0] = vel[0]-mesh_vel[0];
                adv_vel[1] = vel[1]-mesh_vel[1];

                //adv_proj = PI(ro*a.gradU + gradP) considering lumped mass PI() = ()
                adv_proj[index] = area*N[i]*(pr * DN_DX(i,0) +density*(adv_vel[0]*DN_DX(i,0) + adv_vel[1]*DN_DX(i,1))*vel[0]);
                adv_proj[index +1] =  area*N[i]*(pr * DN_DX(i,1) + density*(adv_vel[0]*DN_DX(i,0) + adv_vel[1]*DN_DX(i,1))*vel[1]);

                //div_proj = PI(ro*divU)
                div_proj[i] = area*N[i]*density*(DN_DX(i,0)*vel[0] + DN_DX(i,1)*vel[1]);

                //update projections
                array_1d<double,3>& advtermproj = GetGeometry()[i].FastGetSolutionStepValue(ADVPROJ);
                advtermproj[0]+= tauone*adv_proj[index];
                advtermproj[1]+= tauone*adv_proj[index +1];

                double& divtermproj = GetGeometry()[i].FastGetSolutionStepValue(DIVPROJ);
                divtermproj += tautwo*div_proj[i] ;
		
                //calculate nodal area
                GetGeometry()[i].FastGetSolutionStepValue(NODAL_AREA) += 0.333333333333333333*area;
         }*/


    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::AddProjectionForces(VectorType& F, const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX, const double volume, const double tauone, const double tautwo) {
        unsigned int number_of_nodes = GetGeometry().PointsNumber();
        unsigned int dim = 3;

        double density;
        double mu;
        calculatedensity(GetGeometry(), density, mu);

        const array_1d<double, 3 > advproj_0 = GetGeometry()[0].FastGetSolutionStepValue(ADVPROJ);
        const array_1d<double, 3 > advproj_1 = GetGeometry()[1].FastGetSolutionStepValue(ADVPROJ);
        const array_1d<double, 3 > advproj_2 = GetGeometry()[2].FastGetSolutionStepValue(ADVPROJ);
        const array_1d<double, 3 > advproj_3 = GetGeometry()[3].FastGetSolutionStepValue(ADVPROJ);


        const double div_proj_0 = GetGeometry()[0].FastGetSolutionStepValue(DIVPROJ);
        const double div_proj_1 = GetGeometry()[1].FastGetSolutionStepValue(DIVPROJ);
        const double div_proj_2 = GetGeometry()[2].FastGetSolutionStepValue(DIVPROJ);
        const double div_proj_3 = GetGeometry()[3].FastGetSolutionStepValue(DIVPROJ);



        //mean values
        double mean_x_adv = 0.25 * (advproj_0[0] + advproj_1[0] + advproj_2[0]+ advproj_3[0]);
        double mean_y_adv = 0.25 * (advproj_0[1] + advproj_1[1] + advproj_2[1]+ advproj_3[1]);
        double mean_z_adv = 0.25 * (advproj_0[2] + advproj_1[2] + advproj_2[2]+ advproj_3[2]);

        double mean_div = 0.3333333333333333 * (div_proj_0 + div_proj_1 + div_proj_2 + div_proj_3);

        for (unsigned int ii = 0; ii < number_of_nodes; ii++) {
            int index = ii * (dim + 1);
            const array_1d<double, 3 > & vel = GetGeometry()[ii].FastGetSolutionStepValue(VELOCITY);

            const array_1d<double, 3 > & mesh_vel = GetGeometry()[ii].FastGetSolutionStepValue(MESH_VELOCITY);
            array_1d<double, 2 > adv_vel = ZeroVector(2);
            adv_vel[0] = vel[0] - mesh_vel[0];
            adv_vel[1] = vel[1] - mesh_vel[1];
            adv_vel[2] = vel[2] - mesh_vel[2];
            //tauone*ro*(xi,a.gradv)
            double proj;

            proj = mean_x_adv * (adv_vel[0] * DN_DX(ii, 0) + adv_vel[1] * DN_DX(ii, 1) + adv_vel[2] * DN_DX(ii, 2));
            F[index] += (tauone * 1.0 * volume * proj);

            proj = mean_y_adv * (adv_vel[0] * DN_DX(ii, 0) + adv_vel[1] * DN_DX(ii, 1) + adv_vel[2] * DN_DX(ii, 2));
            F[index + 1 ] += (tauone * 1.0 * volume * proj);

            proj = mean_z_adv * (adv_vel[0] * DN_DX(ii, 0) + adv_vel[1] * DN_DX(ii, 1) + adv_vel[2] * DN_DX(ii, 2));
            F[index + 2 ] += (tauone * 1.0 * volume * proj);

            //tauone*(xi,gradq)
            proj = (mean_x_adv * DN_DX(ii, 0) + mean_y_adv * DN_DX(ii, 1) + mean_z_adv * DN_DX(ii, 2));
            F[index + 3] += (tauone * volume * proj);

            //tautwo*(divv)
	    double fac = tautwo * volume * mean_div;
            F[index] += (fac * DN_DX(ii, 0));
            F[index + 1] += (fac * DN_DX(ii, 1));
            F[index + 2] += (fac * DN_DX(ii, 2));

        }




    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::EquationIdVector(EquationIdVectorType& rResult, ProcessInfo& CurrentProcessInfo) {
        KRATOS_TRY
                unsigned int number_of_nodes = GetGeometry().PointsNumber();
        unsigned int dim = 3;
        unsigned int node_size = dim + 1;


        if (rResult.size() != number_of_nodes * node_size)
            rResult.resize(number_of_nodes * node_size, false);

        for (unsigned int i = 0; i < number_of_nodes; i++) {
            rResult[i * node_size] = GetGeometry()[i].GetDof(VELOCITY_X).EquationId();
            rResult[i * node_size + 1] = GetGeometry()[i].GetDof(VELOCITY_Y).EquationId();
            rResult[i * node_size + 2] = GetGeometry()[i].GetDof(VELOCITY_Z).EquationId();
            rResult[i * node_size + 3] = GetGeometry()[i].GetDof(PRESSURE).EquationId();
        }
        KRATOS_CATCH("")

    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::GetDofList(DofsVectorType& ElementalDofList, ProcessInfo& CurrentProcessInfo) {
        KRATOS_TRY
                unsigned int number_of_nodes = GetGeometry().PointsNumber();
        unsigned int dim = 3;
        unsigned int node_size = dim + 1;


        if (ElementalDofList.size() != number_of_nodes * node_size)
            ElementalDofList.resize(number_of_nodes * node_size);

        for (unsigned int i = 0; i < number_of_nodes; i++) {
            ElementalDofList[i * node_size] = GetGeometry()[i].pGetDof(VELOCITY_X);
            ElementalDofList[i * node_size + 1] = GetGeometry()[i].pGetDof(VELOCITY_Y);
            ElementalDofList[i * node_size + 2] = GetGeometry()[i].pGetDof(VELOCITY_Z);
            ElementalDofList[i * node_size + 3] = GetGeometry()[i].pGetDof(PRESSURE);
        }
        KRATOS_CATCH("");
    }

 //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateB(
            boost::numeric::ublas::bounded_matrix<double, 6, 12 > & B,
            const boost::numeric::ublas::bounded_matrix<double, 4, 3 > & DN_DX) {
        KRATOS_TRY
                unsigned int dim = 3;
        unsigned int node_size = dim + 1;

        for (unsigned int i = 0; i < node_size; i++) {
            unsigned int index = dim*i;

// 	    KRATOS_WATCH(DN_DX)
// 	    KRATOS_WATCH(B)
	    
            B(0, index) = DN_DX(i, 0);
            B(0, index + 1) = 0.0;
	    B(0, index + 2) = 0.0;
            B(1, index) = 0.0;
            B(1, index + 1) = DN_DX(i, 1);
	    B(1, index + 2) =0.0;
            B(2, index) = 0.0;
            B(2, index + 1) = 0.0;
	    B(2, index + 2) = DN_DX(i, 2);
	    B(3, index) = DN_DX(i, 1);
            B(3, index + 1) = DN_DX(i, 0);
	    B(3, index + 2) = 0.0;
	    B(4, index) = 0.0;
            B(4, index + 1) = DN_DX(i, 2);
	    B(4, index + 2) = DN_DX(i, 1);
	    B(5, index) = DN_DX(i, 2);
            B(5, index + 1) = 0.0;
	    B(5, index + 2) = DN_DX(i, 0);
// 	    KRATOS_WATCH(B)
            //CalculateBi(Bi,F,DN_DX,i);
            //MathUtils<double>::WriteMatrix(B,Bi,0,index);
        }
        KRATOS_CATCH("")
    }

    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateGradSymVel(array_1d<double, 6 > & grad_sym_vel, double & grad_sym_vel_norm,
            const boost::numeric::ublas::bounded_matrix<double, 6, 12 > & B) {
        KRATOS_TRY
                unsigned int dim = 3;
        unsigned int nodes_number = dim + 1;

        array_1d<double, 12 > U = ZeroVector(12);
        //		array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
        for (unsigned int ii = 0; ii < nodes_number; ++ii) {
            int index = ii * (dim);
	    const array_1d<double,3>& vel =  GetGeometry()[ii].FastGetSolutionStepValue(VELOCITY);
            U[index] = vel[0];
            U[index + 1] = vel[1];
	    U[index + 2] = vel[2];
        }
// KRATOS_WATCH(B)
// KRATOS_WATCH(U)
        grad_sym_vel = prod(B, U);

// Norm of the gradient of velocity:
//         grad_sym_vel_norm = grad_sym_vel[0] * grad_sym_vel[0] + grad_sym_vel[1] * grad_sym_vel[1] + 0.5 * grad_sym_vel[2] * grad_sym_vel[2];
// Gamma dot found in literature!!!:
        grad_sym_vel_norm = 2.0 * grad_sym_vel[0] * grad_sym_vel[0] + 2.0 * grad_sym_vel[1] * grad_sym_vel[1] + 2.0 * grad_sym_vel[2] * grad_sym_vel[2] + grad_sym_vel[3] * grad_sym_vel[3] + grad_sym_vel[4] * grad_sym_vel[4] + grad_sym_vel[5] * grad_sym_vel[5];

        if (grad_sym_vel_norm > 0.00001) {
            grad_sym_vel_norm = sqrt(grad_sym_vel_norm);
        } else
            grad_sym_vel_norm = 0.0;
	
	//print on gauss point the gamma dot as TEMPERATURE
/*KRATOS_WATCH(grad_sym_vel_norm)
KRATOS_WATCH(grad_sym_vel)*/	
	
        KRATOS_CATCH("")
    }



    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::CalculateApparentViscosity(double & app_mu,
	    array_1d<double, 6 >&  grad_sym_vel,
            const boost::numeric::ublas::bounded_matrix<double, 6, 12 > & B,
            const double & mu) {
        KRATOS_TRY
        // 	        unsigned int dim = 2;
        // 		unsigned int nodes_number = dim + 1;
        app_mu = 0.0;
// 	double yield;

	double grad_sym_vel_norm = 0.0;
//         double friction_angle_tangent = 1; //supposing a 45º friction angle. TO DO --->It should be inserted as a nodal parameter and calculated element by element.
        double mcoef = 300;
// 	double mcoef_inv = 1/mcoef;
	
	double aux_1;
	CalculateGradSymVel(grad_sym_vel, grad_sym_vel_norm, B);
	
	 // The yield is variable: it decreases where water is present
	  unsigned int nodes_number = 4;
	  double yield = 0.0;
	  double water_pressure = 0.0;
	  
	 for (unsigned int ii = 0; ii < nodes_number; ++ii) {
	      if(GetGeometry()[ii].FastGetSolutionStepValue(WATER_PRESSURE) >= 0.0){
		    water_pressure +=  GetGeometry()[ii].FastGetSolutionStepValue(WATER_PRESSURE);
	      }
	      yield +=  GetGeometry()[ii].FastGetSolutionStepValue(YIELD_STRESS);
	  }
	  water_pressure /= nodes_number;
	  yield /= nodes_number;
	  //pay attention: negative yield stress meaningfull
	  if(water_pressure < yield)
	      yield -= water_pressure;
	  else
	      yield = 0.0;
	  
        if (grad_sym_vel_norm > 0.00001) {
            aux_1 = 1.0 - exp(-(mcoef * grad_sym_vel_norm));
// 	    KRATOS_WATCH(aux_1)
// 	    KRATOS_WATCH(grad_sym_vel_norm)
            app_mu = mu + (yield / grad_sym_vel_norm) * aux_1;
            if (app_mu < mu) {
                KRATOS_ERROR(std::logic_error, "!!!!!!!!!!!  APPARENT VISCOSITY < VISCOSITY !!!!!!!!", this->Id());
            }
        } else {
            app_mu = mu + yield*mcoef ;
        }

	
	
        KRATOS_CATCH("")
    }



    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::Calculate(const Variable<array_1d<double, 3 > >& rVariable,
            array_1d<double, 3 > & Output,
            const ProcessInfo& rCurrentProcessInfo) {

        array_1d<double, 12 > adv_proj = ZeroVector(12);
        array_1d<double, 4 > div_proj = ZeroVector(4);

        double delta_t = rCurrentProcessInfo[DELTA_TIME];
	boost::numeric::ublas::bounded_matrix<double, 4, 3 > DN_DX;
        array_1d<double, 4 > N; 
	
        //getting data for the given geometry
        double volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, volume);

        double tauone;
        double tautwo;
        CalculateTau(DN_DX,N,tauone, tautwo, delta_t, volume, rCurrentProcessInfo);

        ComputeProjections(adv_proj, div_proj, DN_DX, tauone, tautwo, N, volume, delta_t);


    }

    //************************************************************************************
    //************************************************************************************

   /* void NoNewtonianASGS3D::GetValueOnIntegrationPoints(const Variable<double>& rVariable, std::vector<double>& rValues, const ProcessInfo& rCurrentProcessInfo) {

        double delta_t = rCurrentProcessInfo[DELTA_TIME];

        //getting data for the given geometry
        double volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, volume);
        double tauone;
        double tautwo;
        CalculateTau(tauone, tautwo, delta_t, volume, rCurrentProcessInfo);
        if (rVariable == THAWONE) {
            for (unsigned int PointNumber = 0;
                    PointNumber < 1;
                    PointNumber++) {
                rValues[PointNumber] = tauone;
            }
        }
        if (rVariable == THAWTWO) {
            for (unsigned int PointNumber = 0;
                    PointNumber < 1; PointNumber++) {

                rValues[PointNumber] = tautwo;
            }
        }

    }*/
    //*************************************************************************************
    //*************************************************************************************

    void NoNewtonianASGS3D::calculatedensity(Geometry< Node < 3 > > geom, double& density, double& viscosity) {

        /*double kk = 0.0;
        for(int ii=0;ii<3;++ii)
                if(geom[ii].GetSolutionStepValue(IS_STRUCTURE) != 1.0)
                        {
                                kk++;
                                density +=geom[ii].FastGetSolutionStepValue(DENSITY);
                        }

        density/=kk;*/
        /*
                density = ZeroVector(3);
        for(int ii=0;ii<3;++ii)
                density[ii] = geom[ii].FastGetSolutionStepValue(DENSITY);*/

        /*const double rho0 = geom[0].FastGetSolutionStepValue(DENSITY);
        const double rho1 = geom[1].FastGetSolutionStepValue(DENSITY);
        const double rho2 = geom[2].FastGetSolutionStepValue(DENSITY);
         density = 0.3333333333333333333333*(rho0 + rho1 + rho2 );*/


        double first = geom[0].FastGetSolutionStepValue(IS_POROUS);
        double second = geom[1].FastGetSolutionStepValue(IS_POROUS);
        double third = geom[2].FastGetSolutionStepValue(IS_POROUS);
        double forth = geom[3].FastGetSolutionStepValue(IS_POROUS);

        density = 0.0;

        if (first == second && second == third && third == forth) {
            //for inside the domain totally inside one fluid
            density = geom[0].FastGetSolutionStepValue(DENSITY);
            viscosity = geom[0].FastGetSolutionStepValue(VISCOSITY);
        } else {
            //for element having common node between two fluids or boundary element with IS_POROUS==1 inside the domain
            for (int ii = 0; ii < 4; ++ii) {
                if (geom[ii].GetSolutionStepValue(IS_POROUS) == 1.0 && geom[ii].GetSolutionStepValue(IS_STRUCTURE) != 1.0) {
                    density = geom[ii].FastGetSolutionStepValue(DENSITY);
                    viscosity = geom[ii].FastGetSolutionStepValue(VISCOSITY);

                }
            }
            //for boundary element with IS_POROUS==1 on the boundary
            if (density == 0.0)
                for (int ii = 0; ii < 4; ++ii) {
                    if (geom[ii].GetSolutionStepValue(IS_POROUS) == 0.0) {
                        density = geom[ii].FastGetSolutionStepValue(DENSITY);
                        viscosity = geom[ii].FastGetSolutionStepValue(VISCOSITY);


                    }
                }

        }


	//Here we calculate Dynamic viscosity from Kinemeatic viscosity
	viscosity *= density;

    }
    //*************************************************************************************
    //*************************************************************************************

    void NoNewtonianASGS3D::CalculateTau(const boost::numeric::ublas::bounded_matrix<double,4,3>& DN_DX, const array_1d<double,4>& N,double& tauone, double& tautwo, const double time, const double volume, const ProcessInfo& rCurrentProcessInfo) {
        KRATOS_TRY
                //calculate mean advective velocity and taus

        const array_1d<double, 3 > & adv_vel0 = GetGeometry()[0].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel0 = GetGeometry()[0].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel1 = GetGeometry()[1].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel1 = GetGeometry()[1].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel2 = GetGeometry()[2].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel2 = GetGeometry()[2].FastGetSolutionStepValue(MESH_VELOCITY);
        const array_1d<double, 3 > & adv_vel3 = GetGeometry()[3].FastGetSolutionStepValue(VELOCITY, 0);
        const array_1d<double, 3 > & mesh_vel3 = GetGeometry()[3].FastGetSolutionStepValue(MESH_VELOCITY);

	array_1d<double, 3 > ms_adv_vel;
        ms_adv_vel[0] = N[0]*(adv_vel0[0] - mesh_vel0[0]) + N[1]*(adv_vel1[0] - mesh_vel1[0]) + N[2]*(adv_vel2[0] - mesh_vel2[0]) + N[3]*(adv_vel3[0] - mesh_vel3[0]);
        ms_adv_vel[1] = N[0]*(adv_vel0[1] - mesh_vel0[1]) + N[1]*(adv_vel1[1] - mesh_vel1[1]) + N[2]*(adv_vel2[1] - mesh_vel2[1]) + N[3]*(adv_vel3[1] - mesh_vel3[1]);
        ms_adv_vel[2] = N[0]*(adv_vel0[2] - mesh_vel0[2]) + N[1]*(adv_vel1[2] - mesh_vel1[2]) + N[2]*(adv_vel2[2] - mesh_vel2[2]) + N[3]*(adv_vel3[2] - mesh_vel3[2]);

        //ms_adv_vel[0] = 0.0;
        //ms_adv_vel[1] = 0.0;


        double advvel_norm = ms_adv_vel[0] * ms_adv_vel[0] + ms_adv_vel[1] * ms_adv_vel[1]+ ms_adv_vel[2] * ms_adv_vel[2];
        advvel_norm = sqrt(advvel_norm);

        double ele_length = pow(12*volume,0.333333333333333333333);  
        ele_length = 2.0/3.0 * ele_length * sqrt(3.00);

        double mu;
        ///*const double mu0 = GetGeometry()[0].FastGetSolutionStepValue(VISCOSITY);
        //const double mu1 = GetGeometry()[1].FastGetSolutionStepValue(VISCOSITY);
        //const double mu2 = GetGeometry()[2].FastGetSolutionStepValue(VISCOSITY);
        //mu = 0.333333333333333333333333*(mu0 + mu1 + mu2);

        double density;
        calculatedensity(GetGeometry(), density, mu);


	boost::numeric::ublas::bounded_matrix<double, 6, 12 > B = ZeroMatrix(6, 12);
	array_1d<double, 6 > grad_sym_vel = ZeroVector(6);


	CalculateB(B, DN_DX);
//         for (int ii = 0; ii < nodes_number; ++ii) {
//             yield += GetGeometry()[ii].FastGetSolutionStepValue(PRESSURE);
//         }

  
	//Bingham
        CalculateApparentViscosity(mu, grad_sym_vel, B, mu);	
	//Newtonian: comment the CalculateApparentViscosity funcion and nothing more (remember to modify CalculateResidual and CalculateViscousTerm
	//do nothing --> we don't need the calculation of grad_sym_vel in this case!!!
	

        const double dyn_st_beta = rCurrentProcessInfo[DYNAMIC_TAU];
        tauone = 1.0 / (dyn_st_beta / time + 4.0 * mu / (ele_length * ele_length * density) + 2.0 * advvel_norm / ele_length);


        tautwo = mu / density + 1.0 * ele_length * advvel_norm / 2.0;



        KRATOS_CATCH("")


    }


    //*************************************************************************************
    //*************************************************************************************

    void NoNewtonianASGS3D::GetFirstDerivativesVector(Vector& values, int Step) {
        const unsigned int number_of_nodes = GetGeometry().size();
        const unsigned int dim = GetGeometry().WorkingSpaceDimension();
        unsigned int MatSize = number_of_nodes * (dim + 1);
        if (values.size() != MatSize) values.resize(MatSize, false);
        for (unsigned int i = 0; i < number_of_nodes; i++) {
            unsigned int index = i * (dim + 1);
            values[index] = GetGeometry()[i].GetSolutionStepValue(VELOCITY_X, Step);
            values[index + 1] = GetGeometry()[i].GetSolutionStepValue(VELOCITY_Y, Step);
            values[index + 2] = GetGeometry()[i].GetSolutionStepValue(VELOCITY_Z, Step);
            values[index + 3] = GetGeometry()[i].GetSolutionStepValue(PRESSURE, Step);

        }
    }
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::GetSecondDerivativesVector(Vector& values, int Step) {
        const unsigned int number_of_nodes = GetGeometry().size();
        const unsigned int dim = GetGeometry().WorkingSpaceDimension();
        unsigned int MatSize = number_of_nodes * (dim + 1);
        if (values.size() != MatSize) values.resize(MatSize, false);
        for (unsigned int i = 0; i < number_of_nodes; i++) {
            unsigned int index = i * (dim + 1);
            values[index] = GetGeometry()[i].GetSolutionStepValue(ACCELERATION_X, Step);
            values[index + 1] = GetGeometry()[i].GetSolutionStepValue(ACCELERATION_Y, Step);
            values[index + 2] = GetGeometry()[i].GetSolutionStepValue(ACCELERATION_Z, Step);
            values[index + 3] = 0.0;
        }
    }
    //************************************************************************************
    //************************************************************************************
    //************************************************************************************
    //************************************************************************************

    void NoNewtonianASGS3D::GetValueOnIntegrationPoints(const Variable<double>& rVariable, std::vector<double>& rValues, const ProcessInfo& rCurrentProcessInfo) {

        double delta_t = rCurrentProcessInfo[DELTA_TIME];
	boost::numeric::ublas::bounded_matrix<double, 4, 3 > DN_DX;
        array_1d<double, 4 > N; 
	
        //getting data for the given geometry
        double Volume;
        GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);
        double tauone;
        double tautwo;
        CalculateTau(DN_DX,N,tauone, tautwo, delta_t, Volume, rCurrentProcessInfo);
        if (rVariable == THAWONE) {
            for (unsigned int PointNumber = 0;
                    PointNumber < 1;
                    PointNumber++) {
                rValues[PointNumber] = tauone;
            }
        }
        if (rVariable == THAWTWO) {
            for (unsigned int PointNumber = 0;
                    PointNumber < 1; PointNumber++) {

                rValues[PointNumber] = tautwo;
            }
        }
        if (rVariable == IS_WATER_ELEMENT) {
            for (unsigned int PointNumber = 0;
                    PointNumber < 1; PointNumber++) {
	//	KRATOS_WATCH(this->GetValue(IS_WATER));
	//	KRATOS_WATCH(this->Info());
                rValues[PointNumber] = this->GetValue(IS_WATER_ELEMENT);
            }
        }
//PROVISIONALbegin---only for debugging
	if (rVariable == TEMPERATURE) {//gamma dot
	  boost::numeric::ublas::bounded_matrix<double, 6, 12> B = ZeroMatrix(6, 12);
	  array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
	  double grad_sym_vel_norm = 0.0;
// 	  double Volume;
// 	  GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);

	  CalculateB(B, DN_DX);      

	  CalculateGradSymVel(grad_sym_vel, grad_sym_vel_norm, B);
	     
            for (unsigned int PointNumber = 0;
                    PointNumber < 1; PointNumber++) {
                rValues[PointNumber] = grad_sym_vel_norm;

            }
        }
// 	if (rVariable == TEMPERATURE) {//1st component of 2*app_mu*grad_sym_vel
// 	  boost::numeric::ublas::bounded_matrix<double, 6, 12 > B = ZeroMatrix(6, 12);
// 	  array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
// 	  double grad_sym_vel_norm = 0.0;
// 	  double mu;
// 	  double density;
// //           double app_mu;
// /*// 	  double Volume;
// // 	  GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);*/
// 	  calculatedensity(GetGeometry(), density, mu);
// 	  CalculateB(B, DN_DX);    
// // 	  CalculateApparentViscosity(app_mu, grad_sym_vel, B, mu);
// 	  CalculateGradSymVel(grad_sym_vel, grad_sym_vel_norm, B);
// 	     
//             for (unsigned int PointNumber = 0;
//                     PointNumber < 1; PointNumber++) {
//                 rValues[PointNumber] = mu*grad_sym_vel[2];
// 
//             }
//         }
	if (rVariable == AUX_INDEX) {//app mu
	  boost::numeric::ublas::bounded_matrix<double, 6, 12 > B = ZeroMatrix(6, 12);
	  array_1d<double, 6 > grad_sym_vel = ZeroVector(6);
// 	  double grad_sym_vel_norm = 0.0;
	  double mu;
	  double density;
          double app_mu;
// 	  double Volume;
// 	  GeometryUtils::CalculateGeometryData(GetGeometry(), DN_DX, N, Volume);
	  calculatedensity(GetGeometry(), density, mu);
	  CalculateB(B, DN_DX);    
	  CalculateApparentViscosity(app_mu, grad_sym_vel, B, mu);
	     
            for (unsigned int PointNumber = 0;
                    PointNumber < 1; PointNumber++) {
                rValues[PointNumber] = app_mu;

            }
        }
//PROVISIONALend---only for debugging
    }
    //*************************************************************************************
    //*************************************************************************************
} // Namespace Kratos


