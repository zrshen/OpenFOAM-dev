/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2023 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "SpalartAllmarasDES.H"
#include "fvModels.H"
#include "fvConstraints.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace LESModels
{

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
tmp<volScalarField> SpalartAllmarasDES<BasicMomentumTransportModel>::chi() const
{
    return volScalarField::New
    (
        typedName("chi"),
        nuTilda_/this->nu()
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField> SpalartAllmarasDES<BasicMomentumTransportModel>::fv1
(
    const volScalarField& chi
) const
{
    const volScalarField chi3("chi3", pow3(chi));
    return volScalarField::New
    (
        typedName("fv1"),
        chi3/(chi3 + pow3(Cv1_))
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal>
SpalartAllmarasDES<BasicMomentumTransportModel>::fv2
(
    const volScalarField::Internal& chi,
    const volScalarField::Internal& fv1
) const
{
    return volScalarField::Internal::New
    (
        typedName("fv2"),
        1.0 - chi/(1.0 + chi*fv1)
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal>
SpalartAllmarasDES<BasicMomentumTransportModel>::Omega
(
    const volTensorField::Internal& gradU
) const
{
    return volScalarField::Internal::New
    (
        typedName("Omega"),
        sqrt(2.0)*mag(skew(gradU))
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal>
SpalartAllmarasDES<BasicMomentumTransportModel>::Stilda
(
    const volScalarField::Internal& chi,
    const volScalarField::Internal& fv1,
    const volScalarField::Internal& Omega,
    const volScalarField::Internal& dTilda
) const
{
    return volScalarField::Internal::New
    (
        typedName("Stilda"),
        max
        (
            Omega
          + fv2(chi, fv1)*nuTilda_()/sqr(kappa_*dTilda),
            Cs_*Omega
        )
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal> SpalartAllmarasDES<BasicMomentumTransportModel>::r
(
    const volScalarField::Internal& nur,
    const volScalarField::Internal& Omega,
    const volScalarField::Internal& dTilda
) const
{
    return volScalarField::Internal::New
    (
        typedName("r"),
        min
        (
            nur
           /(
                max
                (
                    Omega,
                    dimensionedScalar(Omega.dimensions(), small)
                )
               *sqr(kappa_*dTilda)
            ),
            scalar(10)
        )
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal>
SpalartAllmarasDES<BasicMomentumTransportModel>::fw
(
    const volScalarField::Internal& Omega,
    const volScalarField::Internal& dTilda
) const
{
    const volScalarField::Internal r(this->r(nuTilda_, Omega, dTilda));
    const volScalarField::Internal g(typedName("g"), r + Cw2_*(pow6(r) - r));

    return volScalarField::Internal::New
    (
        typedName("fw"),
        g*pow((1 + pow6(Cw3_))/(pow6(g) + pow6(Cw3_)), 1.0/6.0)
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField::Internal>
SpalartAllmarasDES<BasicMomentumTransportModel>::dTilda
(
    const volScalarField::Internal& chi,
    const volScalarField::Internal& fv1,
    const volTensorField::Internal& gradU
) const
{
    return volScalarField::Internal::New
    (
        typedName("dTilda"),
        min(CDES_*this->delta()(), this->y())
    );
}


template<class BasicMomentumTransportModel>
void SpalartAllmarasDES<BasicMomentumTransportModel>::cacheLESRegion
(
    const volScalarField::Internal& dTilda
) const
{
    if (this->mesh_.cacheTemporaryObject(typedName("LESRegion")))
    {
        volScalarField::Internal::New
        (
            typedName("LESRegion"),
            neg(dTilda - this->y()())
        );
    }
}


template<class BasicMomentumTransportModel>
void SpalartAllmarasDES<BasicMomentumTransportModel>::correctNut
(
    const volScalarField& fv1
)
{
    this->nut_ = nuTilda_*fv1;
    this->nut_.correctBoundaryConditions();
    fvConstraints::New(this->mesh_).constrain(this->nut_);
}


template<class BasicMomentumTransportModel>
void SpalartAllmarasDES<BasicMomentumTransportModel>::correctNut()
{
    correctNut(fv1(this->chi()));
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
SpalartAllmarasDES<BasicMomentumTransportModel>::SpalartAllmarasDES
(
    const alphaField& alpha,
    const rhoField& rho,
    const volVectorField& U,
    const surfaceScalarField& alphaRhoPhi,
    const surfaceScalarField& phi,
    const viscosity& viscosity,
    const word& type
)
:
    LESeddyViscosity<BasicMomentumTransportModel>
    (
        type,
        alpha,
        rho,
        U,
        alphaRhoPhi,
        phi,
        viscosity
    ),

    sigmaNut_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "sigmaNut",
            this->coeffDict_,
            0.66666
        )
    ),
    kappa_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "kappa",
            this->coeffDict_,
            0.41
        )
    ),
    Cb1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb1",
            this->coeffDict_,
            0.1355
        )
    ),
    Cb2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cb2",
            this->coeffDict_,
            0.622
        )
    ),
    Cw1_(Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_),
    Cw2_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw2",
            this->coeffDict_,
            0.3
        )
    ),
    Cw3_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cw3",
            this->coeffDict_,
            2.0
        )
    ),
    Cv1_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cv1",
            this->coeffDict_,
            7.1
        )
    ),
    Cs_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "Cs",
            this->coeffDict_,
            0.3
        )
    ),
    CDES_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "CDES",
            this->coeffDict_,
            0.65
        )
    ),
    ck_
    (
        dimensioned<scalar>::lookupOrAddToDict
        (
            "ck",
            this->coeffDict_,
            0.07
        )
    ),

    nuTilda_
    (
        IOobject
        (
            "nuTilda",
            this->runTime_.name(),
            this->mesh_,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        this->mesh_
    )
{
    if (type == typeName)
    {
        this->printCoeffs(type);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
bool SpalartAllmarasDES<BasicMomentumTransportModel>::read()
{
    if (LESeddyViscosity<BasicMomentumTransportModel>::read())
    {
        sigmaNut_.readIfPresent(this->coeffDict());
        kappa_.readIfPresent(*this);

        Cb1_.readIfPresent(this->coeffDict());
        Cb2_.readIfPresent(this->coeffDict());
        Cw1_ = Cb1_/sqr(kappa_) + (1.0 + Cb2_)/sigmaNut_;
        Cw2_.readIfPresent(this->coeffDict());
        Cw3_.readIfPresent(this->coeffDict());
        Cv1_.readIfPresent(this->coeffDict());
        Cs_.readIfPresent(this->coeffDict());

        CDES_.readIfPresent(this->coeffDict());
        ck_.readIfPresent(this->coeffDict());

        return true;
    }
    else
    {
        return false;
    }
}


template<class BasicMomentumTransportModel>
tmp<volScalarField> SpalartAllmarasDES<BasicMomentumTransportModel>::
DnuTildaEff() const
{
    return volScalarField::New
    (
        "DnuTildaEff",
        (nuTilda_ + this->nu())/sigmaNut_
    );
}


template<class BasicMomentumTransportModel>
tmp<volScalarField> SpalartAllmarasDES<BasicMomentumTransportModel>::k() const
{
    const volScalarField chi(this->chi());
    const volScalarField fv1(this->fv1(chi));

    volScalarField dTildaExtrapolated
    (
        IOobject
        (
            "dTildaExtrapolated",
            this->mesh_.time().name(),
            this->mesh_
        ),
        this->mesh_,
        dimLength,
        zeroGradientFvPatchScalarField::typeName
    );
    dTildaExtrapolated.ref() = dTilda(chi, fv1, fvc::grad(this->U_));
    dTildaExtrapolated.correctBoundaryConditions();

    tmp<volScalarField> tk
    (
        volScalarField::New
        (
            typedName("k"),
            sqr(this->nut()/ck_/dTildaExtrapolated)
        )
    );

    const fvPatchList& patches = this->mesh_.boundary();
    volScalarField::Boundary& kBf = tk.ref().boundaryFieldRef();

    forAll(patches, patchi)
    {
        if (isA<wallFvPatch>(patches[patchi]))
        {
            kBf[patchi] = 0;
        }
    }

    return tk;
}


template<class BasicMomentumTransportModel>
void SpalartAllmarasDES<BasicMomentumTransportModel>::correct()
{
    if (!this->turbulence_)
    {
        return;
    }

    // Local references
    const alphaField& alpha = this->alpha_;
    const rhoField& rho = this->rho_;
    const surfaceScalarField& alphaRhoPhi = this->alphaRhoPhi_;
    const volVectorField& U = this->U_;
    const Foam::fvModels& fvModels(Foam::fvModels::New(this->mesh_));
    const Foam::fvConstraints& fvConstraints
    (
        Foam::fvConstraints::New(this->mesh_)
    );

    LESeddyViscosity<BasicMomentumTransportModel>::correct();

    const volScalarField chi(this->chi());
    const volScalarField fv1(this->fv1(chi));

    tmp<volTensorField> tgradU = fvc::grad(U);
    const volScalarField::Internal Omega(this->Omega(tgradU()));
    const volScalarField::Internal dTilda(this->dTilda(chi, fv1, tgradU()));
    const volScalarField::Internal Stilda
    (
        this->Stilda(chi, fv1, Omega, dTilda)
    );
    tgradU.clear();

    tmp<fvScalarMatrix> nuTildaEqn
    (
        fvm::ddt(alpha, rho, nuTilda_)
      + fvm::div(alphaRhoPhi, nuTilda_)
      - fvm::laplacian(alpha*rho*DnuTildaEff(), nuTilda_)
      - Cb2_/sigmaNut_*alpha*rho*magSqr(fvc::grad(nuTilda_))
     ==
        Cb1_*alpha()*rho()*Stilda*nuTilda_()
      - fvm::Sp
        (
            Cw1_*alpha()*rho()*fw(Stilda, dTilda)*nuTilda_()/sqr(dTilda),
            nuTilda_
        )
      + fvModels.source(alpha, rho, nuTilda_)
    );

    nuTildaEqn.ref().relax();
    fvConstraints.constrain(nuTildaEqn.ref());
    solve(nuTildaEqn);
    fvConstraints.constrain(nuTilda_);
    bound(nuTilda_, dimensionedScalar(nuTilda_.dimensions(), 0));
    nuTilda_.correctBoundaryConditions();

    correctNut();

    // Optionally cache the LESRegion field
    cacheLESRegion(dTilda);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace LESModels
} // End namespace Foam

// ************************************************************************* //
