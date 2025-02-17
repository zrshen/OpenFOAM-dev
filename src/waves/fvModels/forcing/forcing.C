/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2017-2023 OpenFOAM Foundation
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

#include "forcing.H"
#include "fvMatrix.H"
#include "Function1Evaluate.H"
#include "fvcGrad.H"
#include "fvcVolumeIntegrate.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(forcing, 0);
}
}


// * * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * //

void Foam::fv::forcing::readLambda()
{
    lambda_ =
        dimensionedScalar
        (
            lambda_.name(),
            lambda_.dimensions(),
            coeffs().lookup(lambda_.name())
        );

    lambdaBoundary_ =
        dimensionedScalar
        (
            lambdaBoundary_.name(),
            lambdaBoundary_.dimensions(),
            coeffs().lookupOrDefault(lambdaBoundary_.name(), 0.0)
        );
}


void Foam::fv::forcing::readCoeffs()
{
    writeForceFields_ = coeffs().lookupOrDefault("writeForceFields", false);

    const bool foundScale = coeffs().found("scale");
    const bool foundOgn = coeffs().found("origin");
    const bool foundDir = coeffs().found("direction");
    const bool foundOgns = coeffs().found("origins");
    const bool foundDirs = coeffs().found("directions");
    const bool foundAll =
        foundScale
     && (
            (foundOgn && foundDir && !foundOgns && !foundDirs)
         || (!foundOgn && !foundDir && foundOgns && foundDirs)
        );
     const bool foundAny =
        foundScale || foundOgn || foundDir || foundOgns || foundDirs;

    if (!foundAll)
    {
        scale_ = autoPtr<Function1<scalar>>();
        origins_.clear();
        directions_.clear();
    }

    if (foundAll)
    {
        scale_ = Function1<scalar>::New("scale", coeffs());
        if (foundOgn)
        {
            origins_.setSize(1);
            directions_.setSize(1);
            coeffs().lookup("origin") >> origins_.last();
            coeffs().lookup("direction") >> directions_.last();
        }
        else
        {
            coeffs().lookup("origins") >> origins_;
            coeffs().lookup("directions") >> directions_;

            if
            (
                origins_.size() == 0
             || directions_.size() == 0
             || origins_.size() != directions_.size()
            )
            {
                FatalErrorInFunction
                    << "The same, non-zero number of origins and "
                    << "directions must be provided" << exit(FatalError);
            }
        }
        forAll(directions_, i)
        {
            directions_[i] /= mag(directions_[i]);
        }
    }

    if (!foundAll && foundAny)
    {
        WarningInFunction
            << "The scaling specification is incomplete. \"scale\", "
            << "\"origin\" and \"direction\" (or \"origins\" and "
            << "\"directions\"), must all be specified in order to scale "
            << "the forcing. The forcing will be applied uniformly across "
            << "the cell set." << endl << endl;
    }
}


Foam::dimensionedScalar Foam::fv::forcing::regionLength() const
{
    dimensionedScalar vs("vs", dimVolume, 0);
    dimensionedScalar vgrads("vs", dimArea, 0);

    forAll(origins_, i)
    {
        const volScalarField x
        (
            (mesh().C() - dimensionedVector(dimLength, origins_[i]))
          & directions_[i]
        );

        const volScalarField scale
        (
            evaluate
            (
                *scale_,
                dimless,
                x
            )
        );

        vs += fvc::domainIntegrate(scale);
        vgrads += fvc::domainIntegrate(directions_[i] & fvc::grad(scale));
    }

    return vs/vgrads;
}



Foam::tmp<Foam::volScalarField::Internal> Foam::fv::forcing::scale() const
{
    tmp<volScalarField::Internal> tscale
    (
        volScalarField::Internal::New
        (
            typedName("scale"),
            mesh(),
            dimensionedScalar(dimless, 0)
        )
    );

    scalarField& scale = tscale.ref();

    forAll(origins_, i)
    {
        const vectorField& c = mesh().cellCentres();
        const scalarField x((c - origins_[i]) & directions_[i]);
        scale = max(scale, scale_->value(x));
    }

    return tscale;
}


Foam::tmp<Foam::volScalarField::Internal> Foam::fv::forcing::forceCoeff() const
{
    tmp<volScalarField::Internal> tscale(this->scale());
    const volScalarField::Internal& scale = tscale();

    tmp<volScalarField::Internal> tforceCoeff
    (
        volScalarField::Internal::New(typedName("forceCoeff"), lambda_*scale)
    );

    // Damp the cells adjacent to the boundary with lambdaBoundary if specified
    if (lambdaBoundary_.value() > 0)
    {
        const fvBoundaryMesh& bm = mesh().boundary();

        forAll(bm, patchi)
        {
            if (!bm[patchi].coupled())
            {
                UIndirectList<scalar>(tforceCoeff.ref(), bm[patchi].faceCells())
              = lambdaBoundary_.value()
               *Field<scalar>(scale, bm[patchi].faceCells());
            }
        }
    }

    return tforceCoeff;
}


void Foam::fv::forcing::writeForceFields() const
{
    if (writeForceFields_)
    {
        Info<< "    Writing forcing fields: forcing:scale, forcing:forceCoeff"
            << endl;

        scale()().write();
        forceCoeff()().write();
    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fv::forcing::forcing
(
    const word& name,
    const word& modelType,
    const fvMesh& mesh,
    const dictionary& dict
)
:
    fvModel(name, modelType, mesh, dict),
    writeForceFields_(false),
    lambda_("lambda", dimless/dimTime, NaN),
    lambdaBoundary_("lambdaBoundary", dimless/dimTime, 0.0),
    scale_(nullptr),
    origins_(),
    directions_()
{
    readCoeffs();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::fv::forcing::read(const dictionary& dict)
{
    if (fvModel::read(dict))
    {
        readCoeffs();
        return true;
    }
    else
    {
        return false;
    }
}


// ************************************************************************* //
