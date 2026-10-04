/**
 *  Product:        Tolk
 *  File:           TolkBehaviour.cs
 *  Description:    Optional Unity component that loads Tolk on Awake and
 *                  unloads it on OnDestroy.
 *  Copyright:      (c) 2014-2026, Tolk contributors
 *  License:        LGPLv3
 */
using UnityEngine;

namespace DavyKager
{
  /// <summary>
  /// Convenience component for Unity scenes. Add it to one GameObject and
  /// Tolk is ready before the first frame; remove it and Tolk is shut down
  /// again. Using <see cref="Tolk"/> directly is equally valid.
  /// </summary>
  [AddComponentMenu("Accessibility/Tolk")]
  [DisallowMultipleComponent]
  public sealed class TolkBehaviour : MonoBehaviour
  {
    [Tooltip("Keep this object (and therefore Tolk) alive across scene loads.")]
    [SerializeField]
    private bool keepAlive = true;

    [Tooltip("Announce a short message once Tolk has loaded.")]
    [SerializeField]
    private bool announceOnStart = false;

    [Tooltip("Message announced when announceOnStart is enabled.")]
    [SerializeField]
    private string announcement = "Tolk is ready";

    private void Awake()
    {
      if (keepAlive)
      {
        DontDestroyOnLoad(gameObject);
      }
      Tolk.Initialize();
    }

    private void Start()
    {
      if (announceOnStart)
      {
        Tolk.Output(announcement);
      }
    }

    private void OnDestroy()
    {
      Tolk.Shutdown();
    }
  }
}