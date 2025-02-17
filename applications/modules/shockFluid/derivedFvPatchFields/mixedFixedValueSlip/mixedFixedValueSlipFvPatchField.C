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

#include "mixedFixedValueSlipFvPatchField.H"
#include "symmTransformField.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class Type>
Foam::mixedFixedValueSlipFvPatchField<Type>::mixedFixedValueSlipFvPatchField
(
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF
)
:
    transformFvPatchField<Type>(p, iF),
    refValue_(p.size()),
    valueFraction_(p.size(), 1.0)
{}


template<class Type>
Foam::mixedFixedValueSlipFvPatchField<Type>::mixedFixedValueSlipFvPatchField
(
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF,
    const dictionary& dict
)
:
    transformFvPatchField<Type>(p, iF),
    refValue_("refValue", dict, p.size()),
    valueFraction_("valueFraction", dict, p.size())
{}


template<class Type>
Foam::mixedFixedValueSlipFvPatchField<Type>::mixedFixedValueSlipFvPatchField
(
    const mixedFixedValueSlipFvPatchField<Type>& ptf,
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF,
    const fieldMapper& mapper
)
:
    transformFvPatchField<Type>(ptf, p, iF, mapper),
    refValue_(mapper(ptf.refValue_)),
    valueFraction_(mapper(ptf.valueFraction_))
{}


template<class Type>
Foam::mixedFixedValueSlipFvPatchField<Type>::mixedFixedValueSlipFvPatchField
(
    const mixedFixedValueSlipFvPatchField<Type>& ptf,
    const DimensionedField<Type, volMesh>& iF
)
:
    transformFvPatchField<Type>(ptf, iF),
    refValue_(ptf.refValue_),
    valueFraction_(ptf.valueFraction_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Type>
void Foam::mixedFixedValueSlipFvPatchField<Type>::map
(
    const fvPatchField<Type>& ptf,
    const fieldMapper& mapper
)
{
    transformFvPatchField<Type>::map(ptf, mapper);

    const mixedFixedValueSlipFvPatchField<Type>& dmptf =
        refCast<const mixedFixedValueSlipFvPatchField<Type>>(ptf);

    mapper(refValue_, dmptf.refValue_);
    mapper(valueFraction_, dmptf.valueFraction_);
}


template<class Type>
void Foam::mixedFixedValueSlipFvPatchField<Type>::reset
(
    const fvPatchField<Type>& ptf
)
{
    transformFvPatchField<Type>::reset(ptf);

    const mixedFixedValueSlipFvPatchField<Type>& dmptf =
        refCast<const mixedFixedValueSlipFvPatchField<Type>>(ptf);

    refValue_.reset(dmptf.refValue_);
    valueFraction_.reset(dmptf.valueFraction_);
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::mixedFixedValueSlipFvPatchField<Type>::snGrad() const
{
    tmp<vectorField> nHat = this->patch().nf();
    Field<Type> pif(this->patchInternalField());

    return
    (
        valueFraction_*refValue_
      + (1.0 - valueFraction_)*transform(I - sqr(nHat), pif) - pif
    )*this->patch().deltaCoeffs();
}


template<class Type>
void Foam::mixedFixedValueSlipFvPatchField<Type>::evaluate
(
    const Pstream::commsTypes
)
{
    if (!this->updated())
    {
        this->updateCoeffs();
    }

    vectorField nHat(this->patch().nf());

    Field<Type>::operator=
    (
        valueFraction_*refValue_
      +
        (1.0 - valueFraction_)
       *transform(I - nHat*nHat, this->patchInternalField())
    );

    transformFvPatchField<Type>::evaluate();
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::mixedFixedValueSlipFvPatchField<Type>::snGradTransformDiag() const
{
    vectorField nHat(this->patch().nf());
    vectorField diag(nHat.size());

    diag.replace(vector::X, mag(nHat.component(vector::X)));
    diag.replace(vector::Y, mag(nHat.component(vector::Y)));
    diag.replace(vector::Z, mag(nHat.component(vector::Z)));

    return
        valueFraction_*Type(pTraits<Type>::one)
      + (1.0 - valueFraction_)
       *transformFieldMask<Type>(pow<vector, pTraits<Type>::rank>(diag));
}


template<class Type>
void Foam::mixedFixedValueSlipFvPatchField<Type>::write(Ostream& os) const
{
    transformFvPatchField<Type>::write(os);
    writeEntry(os, "refValue", refValue_);
    writeEntry(os, "valueFraction", valueFraction_);
}


// ************************************************************************* //
