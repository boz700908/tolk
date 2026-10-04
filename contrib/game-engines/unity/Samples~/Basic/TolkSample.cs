/**
 *  Product:        Tolk
 *  File:           TolkSample.cs
 *  Description:    Minimal Unity sample for the Tolk wrapper.
 *  License:        LGPLv3
 */
using DavyKager;
using UnityEngine;

public sealed class TolkSample : MonoBehaviour
{
  [TextArea]
  public string message = "Hello from Tolk";

  private void Awake()
  {
    Tolk.Initialize();
    Debug.Log("Active screen reader: " + (Tolk.DetectScreenReader() ?? "none"));
    Debug.Log("Speech: " + Tolk.HasSpeech() + ", braille: " + Tolk.HasBraille());
  }

  public void Announce()
  {
    Tolk.Output(message);
  }

  public void ShutUp()
  {
    Tolk.Silence();
  }

  private void OnDestroy()
  {
    Tolk.Shutdown();
  }
}