!
!     CalculiX - A 3-dimensional finite element program
!     Copyright (C) 1998-2025 Guido Dhondt
!
!     This program is free software; you can redistribute it and/or
!     modify it under the terms of the GNU General Public License as
!     published by the Free Software Foundation(version 2);
!
!
!     This program is distributed in the hope that it will be useful,
!     but WITHOUT ANY WARRANTY; without even the implied warranty of
!     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
!     GNU General Public License for more details.
!
!     You should have received a copy of the GNU General Public License
!     along with this program; if not, write to the Free Software
!     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
!
      subroutine mafilldamas(co,kon,ipkon,lakon,ne0,nactdof,
     &     jq,irow,neq,nzs,au,ad,vold,mi,damjac,nmpc,ndamas,dam,
     &     dambase,nskip,nadv,nhole,nfloor)
!
!     UNSYM stage 2: assembles the asymmetric part of the damage-consistent
!     material tangent
!
!         C = g(D)*C_ep - sigma_eff (x) dD/d(eps)
!
!     The first term is already in au: mafillsm.f built it and
!     mafillsmasmain.c mirrored it into the upper half.  This routine adds
!     only the rank-1 correction, so nothing in e_c3d.f, anisonl.f or the
!     21-entry constitutive storage has to change.
!
!     e_c3d.f builds the material part of the element matrix as
!
!         s(ii1+i1-1,jj1+j1-1) += F(i1,m) C(m,k,n,l) F(j1,n)
!                                 * shpj(k,ii) * shpj(l,jj) * weight
!
!     with F = I + vo the deformation gradient (vo is the displacement
!     gradient with respect to the reference configuration) and shpj the
!     shape-function derivatives scaled by sqrt(det J).  Substituting the
!     rank-1 correction C(m,k,n,l) = -T(m,k)*Q(n,l) factorises the element
!     contribution into an outer product of two 3*nope vectors
!
!         a(ii,i1) = F(i1,m) T(m,k) shpj(k,ii)
!         b(jj,j1) = F(j1,n) Q(n,l) shpj(l,jj)
!         s        = -weight * a (x) b
!
!     T is the effective stress and Q the strain derivative of D, both
!     stored per integration point in damjac by resultsmech.f and both
!     expanded here with the Voigt map that anisotropic.f uses:
!     (1,1)->1 (2,2)->2 (3,3)->3 (1,2)->4 (1,3)->5 (2,3)->6.
!
!     Restrictions, checked below rather than assumed:
!       - C3D4 only, matching the DE1/DM2 progressive-damage restriction
!         in calcdamage.f;
!       - no MPCs.  With nmpc=0 a DOF pair involving a constrained DOF
!         contributes nothing to the matrix in mafillsm.f either, so
!         dropping those pairs here is exact.
!
      implicit none
!
      character*8 lakon(*)
      character*32 cargu
!
!     mi has to be declared before it can size the other arrays
!
      integer mi(*)
!
      integer kon(*),ipkon(*),ne0,nactdof(0:mi(2),*),jq(*),irow(*),
     &     neq(*),nzs(3),nmpc,ndamas,nskip,nadv,nhole,nfloor,
     &     i,j,k,l,m,i1,j1,k1,ii,jj,ll,indexe,nope,konl(4),
     &     jdof1,jdof2,iflag
!
      real*8 co(3,*),au(*),ad(*),vold(0:mi(2),*),damjac(12,mi(1),*),
     &     dam(mi(1),*),dambase(mi(1),*),
     &     xl(3,4),voldl(3,4),shp(4,4),shpj(3,4),vo(3,3),fdef(3,3),
     &     tt(3,3),qq(3,3),avec(12),bvec(12),
     &     xi,et,ze,xsj,xsjj,weight,tsum,value,damuscale,damgmin
!
      include "gauss.f"
!
      iflag=2
!
!     Scale on the rank-1 term, for measuring rather than guessing.
!     mpfd validates the 6x6 material tangent - including the shear
!     rows once a shear path is superposed - so damjac is right, and
!     any remaining error is in turning it into the structural
!     operator here.  At the DHC1 stall the line search cannot
!     contract at any damping factor, which says the Newton
!     direction is an ascent direction; a sign or scale error in
!     this term is the obvious candidate and this makes it testable.
!
      damuscale=1.d0
      call getenv('CCX_DAMAGE_UNSYM_SCALE',cargu)
      if(cargu(1:1).ne.' ') then
        read(cargu,*,err=8100,end=8100) damuscale
 8100   continue
      endif
!
!     Residual-stiffness floor, parsed exactly as resultsmech.f does.
!     Above 1-gmin a missing rank-1 term is correct, below it is not,
!     and the two must not be reported as one number.
!
      damgmin=1.d-4
      call getenv('CCX_DAMAGE_GMIN',cargu)
      if(cargu(1:1).ne.' ') then
        read(cargu,*,err=8200,end=8200) damgmin
 8200   continue
        if(damgmin.lt.1.d-6) damgmin=1.d-4
        if(damgmin.gt.0.2d0) damgmin=1.d-4
      endif
      ndamas=0
      nskip=0
      nhole=0
      nfloor=0
      nadv=0
!
      do i=1,ne0
!
        if(ipkon(i).lt.0) cycle
        if(lakon(i)(1:1).ne.'C') cycle
!
!       skip integration points that are not actively softening;
!       resultsmech.f clears damjac for those
!
        tsum=0.d0
        do j=1,12
          tsum=tsum+dabs(damjac(j,1,i))
        enddo
!
!       An element past initiation whose damage is advancing must
!       carry the rank-1 term.  If damjac is empty for such a point
!       the correction is silently missing and the operator is too
!       stiff there - which is exactly what the structural FD probe
!       measures at node 294 of SP1: K_asm/K_fd = 1.05 over twenty
!       entries, stable as the iteration converges.
!
        if(tsum.le.0.d0) then
          if(dam(1,i).gt.1.d0+1.d-12) then
            nskip=nskip+1
!
!           An unloading point legitimately has no rank-1 term,
!           because D is frozen and dD/d(eps) is zero.  A point
!           whose damage is ADVANCING and still has no term is a
!           genuine hole in the tangent.
!
            if(dam(1,i)-dambase(1,i).gt.1.d-14) then
              nadv=nadv+1
!
!             nadv alone is not evidence of a defect: dambase is the
!             damage at the START OF THE INCREMENT, so this branch
!             also holds every point that advanced earlier in the
!             increment and unloads elastically now, where zero is
!             right.  Split off the two bands whose reason is known.
!
              if(dam(1,i).ge.2.d0-damgmin) then
!               on the residual-stiffness floor: dg/dD=0, zero correct
                nfloor=nfloor+1
              elseif(dam(1,i).ge.1.999d0) then
!               past the terminal threshold but not yet floored, so
!               resultsmech refuses the fill and the element carries
!               g*C_ep alone.  This is the genuine hole.
                nhole=nhole+1
              endif
            endif
          endif
          cycle
        endif
!
        if(lakon(i)(4:4).ne.'4') then
          write(*,*) '*ERROR in mafilldamas: the asymmetric damage'
          write(*,*) '       tangent is implemented for C3D4 only.'
          write(*,*) '       Element: ',i
          call exit(201)
        endif
        if(nmpc.gt.0) then
          write(*,*) '*ERROR in mafilldamas: the asymmetric damage'
          write(*,*) '       tangent does not support MPCs yet.'
          call exit(201)
        endif
!
        nope=4
        indexe=ipkon(i)
        do j=1,nope
          konl(j)=kon(indexe+j)
          do k=1,3
            xl(k,j)=co(k,konl(j))
            voldl(k,j)=vold(k,konl(j))
          enddo
        enddo
!
!       C3D4 has a single integration point
!
        xi=gauss3d4(1,1)
        et=gauss3d4(2,1)
        ze=gauss3d4(3,1)
        weight=weight3d4(1)
        call shape4tet(xi,et,ze,xl,xsj,shp,iflag)
        if(xsj.lt.1.d-20) cycle
        xsjj=dsqrt(xsj)
        do j=1,nope
          do k=1,3
            shpj(k,j)=shp(k,j)*xsjj
          enddo
        enddo
!
!       deformation gradient
!
        do i1=1,3
          do j1=1,3
            vo(i1,j1)=0.d0
            do k1=1,nope
              vo(i1,j1)=vo(i1,j1)+shp(j1,k1)*voldl(i1,k1)
            enddo
            fdef(i1,j1)=vo(i1,j1)
          enddo
          fdef(i1,i1)=fdef(i1,i1)+1.d0
        enddo
!
!       effective stress and dD/d(eps) as symmetric 3x3 tensors
!
        tt(1,1)=damjac(1,1,i)
        tt(2,2)=damjac(2,1,i)
        tt(3,3)=damjac(3,1,i)
        tt(1,2)=damjac(4,1,i)
        tt(2,1)=damjac(4,1,i)
        tt(1,3)=damjac(5,1,i)
        tt(3,1)=damjac(5,1,i)
        tt(2,3)=damjac(6,1,i)
        tt(3,2)=damjac(6,1,i)
!
        qq(1,1)=damjac(7,1,i)
        qq(2,2)=damjac(8,1,i)
        qq(3,3)=damjac(9,1,i)
        qq(1,2)=damjac(10,1,i)
        qq(2,1)=damjac(10,1,i)
        qq(1,3)=damjac(11,1,i)
        qq(3,1)=damjac(11,1,i)
        qq(2,3)=damjac(12,1,i)
        qq(3,2)=damjac(12,1,i)
!
!       a and b
!
        do ii=1,nope
          do i1=1,3
            avec(3*(ii-1)+i1)=0.d0
            bvec(3*(ii-1)+i1)=0.d0
            do m=1,3
              do k=1,3
                avec(3*(ii-1)+i1)=avec(3*(ii-1)+i1)
     &               +fdef(i1,m)*tt(m,k)*shpj(k,ii)
                bvec(3*(ii-1)+i1)=bvec(3*(ii-1)+i1)
     &               +fdef(i1,m)*qq(m,k)*shpj(k,ii)
              enddo
            enddo
          enddo
        enddo
!
!       assembling -weight * a (x) b over the full square; the first
!       index of the element matrix is the row, matching the (i,j)
!       convention of add_sm_st_as
!
        do jj=1,3*nope
          j=(jj-1)/3+1
          k=jj-3*(j-1)
          jdof1=nactdof(k,konl(j))
          if(jdof1.le.0) cycle
          if(avec(jj).eq.0.d0) cycle
          do ll=1,3*nope
            l=(ll-1)/3+1
            m=ll-3*(l-1)
            jdof2=nactdof(m,konl(l))
            if(jdof2.le.0) cycle
            value=-damuscale*weight*avec(jj)*bvec(ll)
            if(value.eq.0.d0) cycle
            call add_sm_st_as(au,ad,jq,irow,jdof1,jdof2,value,jj,ll,
     &           nzs)
          enddo
        enddo
        ndamas=ndamas+1
!
      enddo
!
      return
      end
