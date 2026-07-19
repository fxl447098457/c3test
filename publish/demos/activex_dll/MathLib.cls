VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
  Persistable = 0
  DataBindingBehavior = 0
  DataSourceBehavior = 0
  MTSTransactionMode = 0
END
Attribute VB_Name = "MathLib"
Option Explicit

Private m_Pi As Double

Public Sub SetPi(ByVal Value As Double)
    m_Pi = Value
End Sub

Public Function GetPi() As Double
    GetPi = m_Pi
End Function

Public Function CircleArea(ByVal Radius As Double) As Double
    CircleArea = m_Pi * Radius * Radius
End Function

Public Function Factorial(ByVal N As Long) As Long
    If N <= 1 Then
        Factorial = 1
    Else
        Factorial = N * Factorial(N - 1)
    End If
End Function

Public Function IsEven(ByVal N As Long) As Long
    If N Mod 2 = 0 Then
        IsEven = -1
    Else
        IsEven = 0
    End If
End Function
