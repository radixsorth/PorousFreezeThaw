C  Doc. Michal Benes - Runge-Kuttova metoda
C-----------------------------------------------------------------------C
      SUBROUTINE UPDATE_TIME_LEVEL (TB, Y, TE, N1, N2, DELTA, FIRST)
C  Runge-Kutta fourth-order scheme with automatic time stepping         C
      DOUBLE PRECISION Y(-1:401,-1:401),Y0(0:400,0:400),K1(0:400,0:400),
     /K3(0:400,0:400),K4(0:400,0:400),K5(0:400,0:400),EPS,DELTA,Q
      DOUBLE PRECISION STEP,T,TB,TE,T0,H,H03, T_STEP(50000)
      INTEGER SYS_ID, N_TST
      LOGICAL LAST, FIRST
      COMMON /MERSN/ STEP
      COMMON /STACK/ Y0,K1,K3,K4,K5
      COMMON /SYS_DATA/ SYS_ID, N_TST, T_STEP 

      T = TB
      IF (FIRST) GO TO 101
      H = STEP
102   IF (DABS(T-TE).LE.DABS(H)) GO TO 101
      LAST = .FALSE.
      GO TO 103
101   LAST = .TRUE.
      H = TE - T
103   T0 = T
      DO 104 I=0,N1
         DO 104 J=0,N2
104         Y0(I,J) = Y(I,J)
      H03 = H/3.D00
C      write(*,*) 'Writing position: T=', T, ' H=', H
      CALL RIGHT_HAND_SIDE(T, Y, K1)

c      OPEN (55,FILE='k1.out',STATUS='unknown')
c      DO I=0,N1
c         DO J=0,N2
c            WRITE (55,*) I, J, K1(I,J)
c         ENDDO
c      ENDDO
c      CLOSE (55)
      
      DO 105 I=0,N1
         DO 105 J=0,N2
105         Y(I,J) = K1(I,J)*H03 + Y0(I,J)
      T = T0 + H03
      CALL RIGHT_HAND_SIDE(T, Y, K3)
      DO 106 I=0,N1
         DO 106 J=0,N2
106         Y(I,J) = (K1(I,J) + K3(I,J))/2.D00*H03 + Y0(I,J)
      CALL RIGHT_HAND_SIDE(T, Y, K3)
      DO 107 I=0,N1
         DO 107 J=0,N2
107         Y(I,J) = (K1(I,J)*0.375D00+K3(I,J)*1.125D00)*H03+Y0(I,J)
      T = T0 + H/2.D00
      CALL RIGHT_HAND_SIDE(T, Y, K4)
      DO 108 I=0,N1
         DO 108 J=0,N2
108   Y(I,J) = (K1(I,J)*1.5D00-K3(I,J)*4.5D00+K4(I,J)*6.D00)*H03+Y0(I,J)
      T = T0 + H
      CALL RIGHT_HAND_SIDE(T, Y, K5)
      DO 109 I=0,N1
         DO 109 J=0,N2
109   Y(I,J) = ((K1(I,J)+K5(I,J))/2.D00+K4(I,J)*2.D00)*H03+Y0(I,J)
      EPS = 0.D00
C      WRITE (*,*) 'EPS=0 ', EPS
      DO 110 I=0,N1
         DO 110 J=0,N2
         Q = DABS(K1(I,J)*0.2D00-K3(I,J)*0.9D00+K4(I,J)*0.8D00-
     /       K5(I,J)*0.1D00)
110   IF (Q.GT.EPS) EPS = Q
C      write (*,*) 'Eps = ', EPS, ' Q= ', Q
C      write (*,*) 'Writing EPS= ', EPS, ' at T,H = ', T, H
      EPS = EPS*DABS(H03)
C      write (*,'(1x,E20.12)') Q, EPS 
      IF (EPS.LE.DELTA) GO TO 111
      DO 112 I=0,N1
         DO 112 J=0,N2
112         Y(I,J) = Y0(I,J)
      T = T0
      GO TO 113
C here we insert the time step measurement
111   if (SYS_ID.EQ.1) then
         N_TST = N_TST + 1
         T_STEP(N_TST) = H
      endif
      IF (.NOT.LAST) GO TO 114
      IF (FIRST) STEP = H
      RETURN
114   IF (EPS.EQ.0.0) GO TO 101
113   H = (DELTA/EPS)**0.2D00*0.8D00*H
C      write(*,*) 'H changed to ', H
      STEP = H
      FIRST = .FALSE.
      GO TO 102
      END
C-----------------------------------------------------------------------C
