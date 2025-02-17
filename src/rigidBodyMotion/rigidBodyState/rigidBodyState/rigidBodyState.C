/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2019-2024 OpenFOAM Foundation
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

#include "rigidBodyState.H"
#include "fvMeshMoversMotionSolver.H"
#include "motionSolver.H"
#include "unitConversion.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace functionObjects
{
    defineTypeNameAndDebug(rigidBodyState, 0);

    addToRunTimeSelectionTable
    (
        functionObject,
        rigidBodyState,
        dictionary
    );
}
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

const Foam::RBD::rigidBodyMotion&
Foam::functionObjects::rigidBodyState::motion() const
{
    const fvMeshMovers::motionSolver& mover =
        refCast<const fvMeshMovers::motionSolver>(mesh_.mover());

    return (refCast<const RBD::rigidBodyMotion>(mover.motion()));
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::functionObjects::rigidBodyState::rigidBodyState
(
    const word& name,
    const Time& runTime,
    const dictionary& dict
)
:
    fvMeshFunctionObject(name, runTime, dict),
    logFiles(obr_, name),
    names_(motion().movingBodyNames())
{
    read(dict);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::functionObjects::rigidBodyState::~rigidBodyState()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::functionObjects::rigidBodyState::read(const dictionary& dict)
{
    fvMeshFunctionObject::read(dict);

    angleUnits_ = dict.lookupOrDefaultBackwardsCompatible<word>
    (
        {"angleUnits", "angleFormat"},
        "radians"
    );

    resetNames(names_);

    return true;
}


void Foam::functionObjects::rigidBodyState::writeFileHeader(const label i)
{
    writeHeader(this->files()[i], "Motion State");
    writeHeaderValue(this->files()[i], "Angle Units", angleUnits_);
    writeCommented(this->files()[i], "Time");

    this->files()[i]<< tab
        << "Centre of rotation" << tab
        << "Orientation" << tab
        << "Linear velocity" << tab
        << "Angular velocity" << endl;

}


bool Foam::functionObjects::rigidBodyState::execute()
{
    return true;
}


bool Foam::functionObjects::rigidBodyState::write()
{
    logFiles::write();

    if (Pstream::master())
    {
        const RBD::rigidBodyMotion& motion = this->motion();

        forAll(names_, i)
        {
            const label bodyID = motion.bodyIndex(names_[i]);

            const spatialTransform CofR(motion.X0(bodyID));
            const spatialVector vCofR(motion.v(bodyID, Zero));

            vector rotationAngle
            (
                quaternion(CofR.E()).eulerAngles(quaternion::XYZ)
            );

            vector angularVelocity(vCofR.w());

            if (angleUnits_ == "degrees")
            {
                rotationAngle.x() = radToDeg(rotationAngle.x());
                rotationAngle.y() = radToDeg(rotationAngle.y());
                rotationAngle.z() = radToDeg(rotationAngle.z());

                angularVelocity.x() = radToDeg(angularVelocity.x());
                angularVelocity.y() = radToDeg(angularVelocity.y());
                angularVelocity.z() = radToDeg(angularVelocity.z());
            }

            writeTime(files()[i]);
            files()[i]
                << tab
                << CofR.r()  << tab
                << rotationAngle  << tab
                << vCofR.l() << tab
                << angularVelocity << endl;
        }
    }

    return true;
}


// ************************************************************************* //
